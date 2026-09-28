--TEST--
netsyslog: LOG_PID, a priority carrying its own facility, the defaults without openlog(), closelog()
--EXTENSIONS--
netsyslog
--INI--
netsyslog.host=127.0.0.1
netsyslog.port=51514
netsyslog.hostname=bad host:name
--FILE--
<?php
require __DIR__ . '/_recv.inc';
$s = netsyslog_server();
syslog(LOG_ERR, 'no openlog');
openlog('my app', LOG_PID, LOG_LOCAL1);
syslog(LOG_INFO, 'with pid');
syslog(LOG_AUTH | LOG_NOTICE, 'own facility');
closelog();
syslog(LOG_DEBUG, 'after closelog');
$pid = getmypid();
foreach (netsyslog_read($s, 4) as $d) { echo str_replace("[$pid]", '[PID]', $d), "\n"; }
?>
--EXPECTF--
<11>%s %d:%d:%d bad_host_name php: no openlog
<142>%s %d:%d:%d bad_host_name my_app[PID]: with pid
<37>%s %d:%d:%d bad_host_name my_app[PID]: own facility
<15>%s %d:%d:%d bad_host_name php: after closelog
