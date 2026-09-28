--TEST--
netsyslog: an unresolvable host costs one lookup, no warning, and syslog() still returns true
--EXTENSIONS--
netsyslog
--INI--
netsyslog.host=no-such-host.invalid
--FILE--
<?php
$t = hrtime(true);
for ($i = 0; $i < 1000; $i++) { $r = syslog(LOG_INFO, 'dropped'); }
var_dump($r);
// One failed lookup, then the retry interval: 1000 calls stay far below a second.
var_dump((hrtime(true) - $t) / 1e9 < 1.0);
?>
--EXPECT--
bool(true)
bool(true)
