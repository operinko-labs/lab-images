--TEST--
netsyslog: without a host the stock functions run and nothing is sent
--EXTENSIONS--
netsyslog
--INI--
netsyslog.host=
--FILE--
<?php
require __DIR__ . '/_recv.inc';
$s = netsyslog_server();
var_dump(openlog('drupal', LOG_NDELAY, LOG_LOCAL0));
var_dump(syslog(LOG_WARNING, 'stock path'));
var_dump(closelog());
var_dump(count(netsyslog_read($s, 1)));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
int(0)
