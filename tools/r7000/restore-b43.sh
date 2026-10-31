#!/bin/sh
# no R7000 depois de um reboot: restaura o kit do b43 de /root/b43kit para /tmp e sobe o AP b43-r7000
K=/root/b43kit
mkdir -p /tmp/wpx /tmp/iwx; cp -a $K/b43 $K/fw /tmp/; cp -a $K/wpx/r /tmp/wpx/; cp -a $K/iwx/r /tmp/iwx/; cp $K/hostapd.conf $K/wpa.conf $K/sta.sh /tmp/
export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib:/tmp/iwx/r/lib
IW=/tmp/iwx/r/usr/sbin/iw
echo -n /tmp/fw > /sys/module/firmware_class/parameters/path
wifi down; sleep 2; killall nas 2>/dev/null; rmmod wl; sleep 1
echo 0000:01:00.0 > /sys/bus/pci/drivers/bcma-pci-bridge/bind 2>/dev/null; sleep 1
for m in compat cfg80211 mac80211 b43; do insmod /tmp/b43/$m.ko || echo "FALHA $m"; done; sleep 3
$IW reg reload; $IW reg set BR
P=$(ls /sys/class/ieee80211/ | tail -1); $IW phy $P interface add wlap0 type __ap
start-stop-daemon -S -b -n hostapd -x /tmp/wpx/r/usr/sbin/hostapd -- /tmp/hostapd.conf; sleep 6
$IW dev wlap0 info | grep -E "ssid|channel"
/tmp/iwx/r/usr/bin/iperf3 -s -D -B 192.168.1.70
