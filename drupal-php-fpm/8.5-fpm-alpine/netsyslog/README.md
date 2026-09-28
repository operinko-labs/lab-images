# netsyslog

PHP's `openlog()`, `syslog()` and `closelog()` over UDP. The stock functions
write to `/dev/log`, which a container can only provide by creating a socket
as root at start. With `netsyslog.host` set they send RFC 3164 datagrams
instead:

    <PRI>Mmm dd hh:mm:ss HOSTNAME IDENT[PID]: MESSAGE

Lines are split and escaped exactly as PHP's own `syslog()` does
(`syslog.filter`), so every line is one datagram. Sending never blocks; a
datagram the kernel cannot take at once is dropped. With `netsyslog.host`
empty the stock functions run unchanged.

| INI | Default | |
| --- | --- | --- |
| `netsyslog.host` | empty (off) | receiver host name or address |
| `netsyslog.port` | 514 | UDP port |
| `netsyslog.hostname` | the container's hostname | HOSTNAME field |
| `netsyslog.max_length` | 8192 | longest datagram in bytes (480–65000) |

The image sets `netsyslog.host` and `netsyslog.hostname` from the
`NETSYSLOG_HOST` and `NETSYSLOG_HOSTNAME` environment variables.

Tests:

    docker run --rm -v "$PWD:/src:ro" php:8.5-fpm-alpine sh -c \
      'apk add -q $PHPIZE_DEPS && cp -r /src /b && cd /b && phpize && ./configure --enable-netsyslog && make && NO_INTERACTION=1 make test TESTS=tests/'
