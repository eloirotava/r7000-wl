#!/bin/sh
# no R7000: recarrega o b43 com os args de $1, associa na AP31-IoT e mede TX/RX com o c5 (iperf3 -s la)
export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib:/tmp/wpx/r/usr/lib:/tmp/iwx/r/lib
IW=/tmp/iwx/r/usr/sbin/iw; I=/tmp/iwx/r/usr/bin/iperf3
killall hostapd wpa_supplicant wpad 2>/dev/null; sleep 1
rmmod b43; insmod /tmp/b43/b43.ko $1 || echo FALHA; sleep 3
$IW reg reload; $IW reg set BR
P=$(ls /sys/class/ieee80211/|tail -1); $IW phy $P interface add wlb0 type managed; ip link set wlb0 up
/tmp/wpx/r/usr/sbin/wpa_supplicant -B -D nl80211 -i wlb0 -c /tmp/wpa.conf >/dev/null
n=0; until $IW dev wlb0 link | grep -q Connected; do n=$((n+1)); [ $n -gt 30 ] && { echo SEM ASSOC; exit 1; }; sleep 1; done
ip addr add 192.168.1.252/32 dev wlb0; ip route replace 192.168.1.2/32 dev wlb0 src 192.168.1.252; sleep 2
dmesg -c >/dev/null
tx=$($I -c 192.168.1.2 -B 192.168.1.252 -t 6 2>&1 | awk '/receiver/{print $7,$8}')
st=$(dmesg | grep "AC txstatus" | awk '{v=$9; p=substr(v,7,2); s=substr(v,5,2); if (p!="00" && s=="00") f++; t++} END{printf "agregados %d, 1a tentativa perdida %d (%.0f%%)", t, f, 100*f/(t?t:1)}')
rx=$($I -c 192.168.1.2 -B 192.168.1.252 -t 6 -R 2>&1 | awk '/receiver/{print $7,$8}')
echo "[$1] TX $tx | RX $rx | $st | $($IW dev wlb0 station dump | awk '/tx bitrate/{print $3,$4,$5,$6}')"
