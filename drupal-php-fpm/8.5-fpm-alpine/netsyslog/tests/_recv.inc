<?php
function netsyslog_server() {
  $s = stream_socket_server('udp://127.0.0.1:51514', $e, $es, STREAM_SERVER_BIND);
  if (!$s) { die("bind failed: $es\n"); }
  return $s;
}
function netsyslog_read($s, int $n): array {
  $out = [];
  $r = [$s]; $w = $x = null;
  while (count($out) < $n && stream_select($r, $w, $x, 1)) {
    $out[] = stream_socket_recvfrom($s, 65535);
    $r = [$s];
  }
  return $out;
}
