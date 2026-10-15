#!/bin/sh
# Na VPS: mede na box BCM4335 (AP no canal 44) o tempo de canal ocupado
# antes e durante uma rajada de probes do b43 (R7000, wlb0).
B=root@10.8.0.20
R=root@10.8.0.11
sv() { timeout 15 ssh $B 'iw dev wlan0 survey dump | grep -A6 "5220 MHz" | grep -E "active time|busy time|receive time|noise"'; }
echo "== base"; sv; sleep 10; echo "== base +10 s"; sv
timeout 60 ssh $R 'export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib; i=0; while [ $i -lt 20 ]; do /tmp/iwx/r/usr/sbin/iw dev wlb0 scan freq 5220 ssid b43-teste >/dev/null 2>&1; i=$((i+1)); done; D=/sys/kernel/debug/b43/phy0; echo "0x1 0xe0" > $D/shm16read; echo "txallfrm $(cat $D/shm16read)"'
echo "== depois da rajada"; sv
