PHP_ARG_ENABLE([netsyslog],
  [whether to enable netsyslog],
  [AS_HELP_STRING([--enable-netsyslog], [Send syslog() over UDP instead of /dev/log])],
  [no])

if test "$PHP_NETSYSLOG" != "no"; then
  PHP_NEW_EXTENSION(netsyslog, netsyslog.c, $ext_shared)
fi
