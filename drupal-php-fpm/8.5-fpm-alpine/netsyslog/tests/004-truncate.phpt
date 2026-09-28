--TEST--
netsyslog: a datagram never exceeds netsyslog.max_length
--EXTENSIONS--
netsyslog
--INI--
netsyslog.host=127.0.0.1
netsyslog.port=51514
netsyslog.hostname=h
netsyslog.max_length=600
--FILE--
<?php
require __DIR__ . '/_recv.inc';
$s = netsyslog_server();
openlog('drupal', 0, LOG_LOCAL0);
syslog(LOG_INFO, str_repeat('x', 5000));
[$d] = netsyslog_read($s, 1);
var_dump(strlen($d), str_ends_with($d, 'xxx'));
?>
--EXPECT--
int(600)
bool(true)
