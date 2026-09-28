#ifndef PHP_NETSYSLOG_H
#define PHP_NETSYSLOG_H

extern zend_module_entry netsyslog_module_entry;
#define phpext_netsyslog_ptr &netsyslog_module_entry

#define PHP_NETSYSLOG_VERSION "1.0.0"

ZEND_BEGIN_MODULE_GLOBALS(netsyslog)
	/* INI */
	char      *host;
	zend_long  port;
	char      *hostname;
	zend_long  max_length;
	/* Per request, like PHP's own openlog() state. */
	char      *ident;
	zend_long  option;
	zend_long  facility;
ZEND_END_MODULE_GLOBALS(netsyslog)

#define NETSYSLOG_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(netsyslog, v)

#endif
