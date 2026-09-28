--TEST--
netsyslog: RFC 3164 as syslog-gateway parses it, openlog() facility, one datagram per line, PHP's escaping
--EXTENSIONS--
netsyslog
--INI--
netsyslog.host=127.0.0.1
netsyslog.port=51514
netsyslog.hostname=testhost
syslog.filter=no-ctrl
--FILE--
<?php
require __DIR__ . '/_recv.inc';
$s = netsyslog_server();
openlog('drupal', LOG_NDELAY, LOG_LOCAL0);
var_dump(syslog(LOG_WARNING, "https://example.fi|1790000000|php|10.0.0.1|/x||0||first line\nsecond\x01line\tä"));
// The two regexes syslog-gateway (Fluent Bit) applies: the envelope, then
// syslog-rfc3164-body on what the envelope leaves.
$envelope = '/^(?:<(?<pri>\d+)>)?\s*(?<rest>.+)$/';
$body = '/^(?<timestamp>[A-Z][a-z]{2}\s+\d{1,2}\s\d{2}:\d{2}:\d{2})\s(?<host>[^\s]+)\s(?<app>[^:\s]+)(?:\[(?<procid>[^\]]+)\])?:?\s?(?<log>.*)$/';
foreach (netsyslog_read($s, 2) as $d) {
  preg_match($envelope, $d, $e);
  preg_match($body, $e['rest'], $b);
  printf("pri=%s host=%s app=%s log=%s stamp_ok=%d\n", $e['pri'], $b['host'], $b['app'], $b['log'], (int) preg_match('/^[A-Z][a-z]{2} [ 1-3]\d \d{2}:\d{2}:\d{2}$/', $b['timestamp']));
}
?>
--EXPECT--
bool(true)
pri=132 host=testhost app=drupal log=https://example.fi|1790000000|php|10.0.0.1|/x||0||first line stamp_ok=1
pri=132 host=testhost app=drupal log=second\x01line\x09ä stamp_ok=1
