/*
 * netsyslog: PHP's openlog()/syslog()/closelog() over UDP.
 *
 * The stock functions write to /dev/log through libc, which in a container
 * needs a root-owned socket at start. With netsyslog.host set, these three
 * functions send RFC 3164 datagrams to that host instead:
 *
 *   <PRI>Mmm dd hh:mm:ss HOSTNAME IDENT[PID]: MESSAGE
 *
 * The message goes through the same line splitting and escaping as PHP's own
 * syslog() (the syslog.filter setting), so every line is one datagram, as it
 * was through /dev/log. With netsyslog.host empty the stock functions run
 * unchanged. Sending never blocks: a datagram the kernel cannot take at once
 * is dropped, as syslog over UDP always may be.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_globals.h"
#include "php_syslog.h"
#include "ext/standard/info.h"
#include "zend_smart_string.h"
#include "php_netsyslog.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

ZEND_DECLARE_MODULE_GLOBALS(netsyslog)

/* After a failed lookup or socket, wait this long before trying again, so a
 * missing gateway costs one attempt per interval rather than one per line. */
#define NETSYSLOG_RETRY_SECONDS 10
#define NETSYSLOG_DEFAULT_PORT 514
#define NETSYSLOG_DEFAULT_MAX 8192
#define NETSYSLOG_MIN_MAX 480
#define NETSYSLOG_MAX_MAX 65000
#define NETSYSLOG_IDENT_MAX 48

static zif_handler orig_openlog;
static zif_handler orig_syslog;
static zif_handler orig_closelog;

/* Process state: one socket per php-fpm worker, opened on first use. */
static int sock_fd = -1;
static struct sockaddr_storage sock_addr;
static socklen_t sock_addrlen;
static time_t sock_failed_at;
static char local_hostname[256];

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("netsyslog.host", "", PHP_INI_SYSTEM, OnUpdateString, host, zend_netsyslog_globals, netsyslog_globals)
	STD_PHP_INI_ENTRY("netsyslog.port", "514", PHP_INI_SYSTEM, OnUpdateLong, port, zend_netsyslog_globals, netsyslog_globals)
	STD_PHP_INI_ENTRY("netsyslog.hostname", "", PHP_INI_SYSTEM, OnUpdateString, hostname, zend_netsyslog_globals, netsyslog_globals)
	STD_PHP_INI_ENTRY("netsyslog.max_length", "8192", PHP_INI_SYSTEM, OnUpdateLong, max_length, zend_netsyslog_globals, netsyslog_globals)
PHP_INI_END()

static bool netsyslog_enabled(void)
{
	return NETSYSLOG_G(host) && NETSYSLOG_G(host)[0] != '\0';
}

static void netsyslog_close_socket(void)
{
	if (sock_fd >= 0) {
		close(sock_fd);
		sock_fd = -1;
	}
}

static bool netsyslog_open_socket(void)
{
	struct addrinfo hints, *res = NULL, *ai;
	char port[16];
	zend_long p = NETSYSLOG_G(port);
	time_t now;

	if (sock_fd >= 0) {
		return true;
	}
	now = time(NULL);
	if (sock_failed_at && now - sock_failed_at < NETSYSLOG_RETRY_SECONDS) {
		return false;
	}

	snprintf(port, sizeof(port), "%ld", (long) (p > 0 && p < 65536 ? p : NETSYSLOG_DEFAULT_PORT));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV;
	if (getaddrinfo(NETSYSLOG_G(host), port, &hints, &res) != 0) {
		sock_failed_at = now;
		return false;
	}
	for (ai = res; ai; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
		if (fd < 0) {
			continue;
		}
		memcpy(&sock_addr, ai->ai_addr, ai->ai_addrlen);
		sock_addrlen = ai->ai_addrlen;
		sock_fd = fd;
		break;
	}
	freeaddrinfo(res);
	if (sock_fd < 0) {
		sock_failed_at = now;
		return false;
	}
	sock_failed_at = 0;
	return true;
}

/* A header field must not contain a space or a colon, or the receiver splits
 * the line in the wrong place. */
static void netsyslog_append_token(smart_string *buf, const char *s, size_t max)
{
	size_t n = 0;
	for (; *s && n < max; s++, n++) {
		char c = *s;
		smart_string_appendc(buf, (c == ' ' || c == ':' || (unsigned char) c < 0x21) ? '_' : c);
	}
}

static const char *netsyslog_hostname(void)
{
	if (NETSYSLOG_G(hostname) && NETSYSLOG_G(hostname)[0] != '\0') {
		return NETSYSLOG_G(hostname);
	}
	if (local_hostname[0] == '\0') {
		if (gethostname(local_hostname, sizeof(local_hostname) - 1) != 0 || local_hostname[0] == '\0') {
			strcpy(local_hostname, "localhost");
		}
	}
	return local_hostname;
}

/* One datagram: the header, then as much of the line as fits. */
static void netsyslog_send(zend_long priority, const char *line, size_t len)
{
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	smart_string buf = {0};
	zend_long max = NETSYSLOG_G(max_length);
	int pri = (int) priority;
	time_t now;
	struct tm tm;
	char stamp[32];

	if (!netsyslog_open_socket()) {
		return;
	}
	if (max <= 0) {
		max = NETSYSLOG_DEFAULT_MAX;
	} else if (max < NETSYSLOG_MIN_MAX) {
		max = NETSYSLOG_MIN_MAX;
	} else if (max > NETSYSLOG_MAX_MAX) {
		max = NETSYSLOG_MAX_MAX;
	}

	/* Like libc: a priority without a facility takes openlog()'s, and
	 * without openlog() that is LOG_USER. */
	if ((pri & LOG_FACMASK) == 0) {
		pri |= (int) (NETSYSLOG_G(facility) ? NETSYSLOG_G(facility) : LOG_USER);
	}
	pri &= LOG_FACMASK | LOG_PRIMASK;

	now = time(NULL);
	localtime_r(&now, &tm);
	/* Month names are fixed English, whatever the process locale. */
	snprintf(stamp, sizeof(stamp), "%s %2d %02d:%02d:%02d", months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	smart_string_appendc(&buf, '<');
	smart_string_append_long(&buf, pri);
	smart_string_appendc(&buf, '>');
	smart_string_appends(&buf, stamp);
	smart_string_appendc(&buf, ' ');
	netsyslog_append_token(&buf, netsyslog_hostname(), 255);
	smart_string_appendc(&buf, ' ');
	netsyslog_append_token(&buf, NETSYSLOG_G(ident) ? NETSYSLOG_G(ident) : "php", NETSYSLOG_IDENT_MAX);
	if (NETSYSLOG_G(option) & LOG_PID) {
		smart_string_appendc(&buf, '[');
		smart_string_append_long(&buf, (zend_long) getpid());
		smart_string_appendc(&buf, ']');
	}
	smart_string_appendl(&buf, ": ", 2);
	if (buf.len < (size_t) max) {
		smart_string_appendl(&buf, line, MIN(len, (size_t) max - buf.len));
	}

	if (sendto(sock_fd, buf.c, buf.len, MSG_DONTWAIT | MSG_NOSIGNAL, (struct sockaddr *) &sock_addr, sock_addrlen) < 0) {
		/* A full buffer drops this line; anything else may be a stale
		 * address, so look the host up again next time. */
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENOBUFS) {
			netsyslog_close_socket();
		}
	}
	smart_string_free(&buf);
}

/* The body of php_syslog_str() (main/php_syslog.c), sending each line with
 * netsyslog_send() instead of libc syslog(). */
static void netsyslog_syslog_str(zend_long priority, const zend_string *message)
{
	smart_string sbuf = {0};

	if (PG(syslog_filter) == PHP_SYSLOG_FILTER_RAW) {
		netsyslog_send(priority, ZSTR_VAL(message), ZSTR_LEN(message));
		return;
	}

	for (size_t i = 0; i < ZSTR_LEN(message); ++i) {
		unsigned char c = ZSTR_VAL(message)[i];

		if ((0x20 <= c) && (c <= 0x7e)) {
			smart_string_appendc(&sbuf, c);
		} else if ((c >= 0x80) && (PG(syslog_filter) != PHP_SYSLOG_FILTER_ASCII)) {
			smart_string_appendc(&sbuf, c);
		} else if (c == '\n') {
			netsyslog_send(priority, sbuf.c ? sbuf.c : "", sbuf.len);
			smart_string_reset(&sbuf);
		} else if ((c < 0x20) && (PG(syslog_filter) == PHP_SYSLOG_FILTER_ALL)) {
			smart_string_appendc(&sbuf, c);
		} else {
			static const char xdigits[] = "0123456789abcdef";

			smart_string_appendl(&sbuf, "\\x", 2);
			smart_string_appendc(&sbuf, xdigits[c >> 4]);
			smart_string_appendc(&sbuf, xdigits[c & 0xf]);
		}
	}

	netsyslog_send(priority, sbuf.c ? sbuf.c : "", sbuf.len);
	smart_string_free(&sbuf);
}

static void netsyslog_reset_ident(void)
{
	if (NETSYSLOG_G(ident)) {
		free(NETSYSLOG_G(ident));
		NETSYSLOG_G(ident) = NULL;
	}
	NETSYSLOG_G(option) = 0;
	NETSYSLOG_G(facility) = 0;
}

static PHP_FUNCTION(netsyslog_openlog)
{
	char *ident;
	size_t ident_len;
	zend_long option, facility;

	if (!netsyslog_enabled()) {
		orig_openlog(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_STRING(ident, ident_len)
		Z_PARAM_LONG(option)
		Z_PARAM_LONG(facility)
	ZEND_PARSE_PARAMETERS_END();

	netsyslog_reset_ident();
	NETSYSLOG_G(ident) = zend_strndup(ident, ident_len);
	NETSYSLOG_G(option) = option;
	NETSYSLOG_G(facility) = facility & LOG_FACMASK;
	RETURN_TRUE;
}

static PHP_FUNCTION(netsyslog_syslog)
{
	zend_long priority;
	zend_string *message;

	if (!netsyslog_enabled()) {
		orig_syslog(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(priority)
		Z_PARAM_STR(message)
	ZEND_PARSE_PARAMETERS_END();

	netsyslog_syslog_str(priority, message);
	RETURN_TRUE;
}

static PHP_FUNCTION(netsyslog_closelog)
{
	if (!netsyslog_enabled()) {
		orig_closelog(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	ZEND_PARSE_PARAMETERS_NONE();

	netsyslog_reset_ident();
	RETURN_TRUE;
}

static void netsyslog_swap(const char *name, zif_handler replacement, zif_handler *original)
{
	zend_function *fn = zend_hash_str_find_ptr(CG(function_table), name, strlen(name));

	if (fn && fn->type == ZEND_INTERNAL_FUNCTION) {
		*original = fn->internal_function.handler;
		fn->internal_function.handler = replacement;
	}
}

static void netsyslog_restore(const char *name, zif_handler original)
{
	zend_function *fn;

	if (!original) {
		return;
	}
	fn = zend_hash_str_find_ptr(CG(function_table), name, strlen(name));
	if (fn && fn->type == ZEND_INTERNAL_FUNCTION) {
		fn->internal_function.handler = original;
	}
}

static PHP_GINIT_FUNCTION(netsyslog)
{
#if defined(COMPILE_DL_NETSYSLOG) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	memset(netsyslog_globals, 0, sizeof(*netsyslog_globals));
}

static PHP_MINIT_FUNCTION(netsyslog)
{
	REGISTER_INI_ENTRIES();
	netsyslog_swap("openlog", ZEND_FN(netsyslog_openlog), &orig_openlog);
	netsyslog_swap("syslog", ZEND_FN(netsyslog_syslog), &orig_syslog);
	netsyslog_swap("closelog", ZEND_FN(netsyslog_closelog), &orig_closelog);
	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(netsyslog)
{
	netsyslog_restore("openlog", orig_openlog);
	netsyslog_restore("syslog", orig_syslog);
	netsyslog_restore("closelog", orig_closelog);
	netsyslog_close_socket();
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

/* PHP forgets openlog() at the end of every request; so does this. */
static PHP_RSHUTDOWN_FUNCTION(netsyslog)
{
	netsyslog_reset_ident();
	return SUCCESS;
}

static PHP_MINFO_FUNCTION(netsyslog)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "netsyslog", netsyslog_enabled() ? "enabled (UDP)" : "disabled (stock syslog)");
	php_info_print_table_row(2, "Version", PHP_NETSYSLOG_VERSION);
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}

zend_module_entry netsyslog_module_entry = {
	STANDARD_MODULE_HEADER,
	"netsyslog",
	NULL,
	PHP_MINIT(netsyslog),
	PHP_MSHUTDOWN(netsyslog),
	NULL,
	PHP_RSHUTDOWN(netsyslog),
	PHP_MINFO(netsyslog),
	PHP_NETSYSLOG_VERSION,
	PHP_MODULE_GLOBALS(netsyslog),
	PHP_GINIT(netsyslog),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_NETSYSLOG
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(netsyslog)
#endif
