#!/bin/sh
# no R7000 depois de um reboot: restaura o kit do b43 de /root/b43kit para /tmp e sobe os APs
# b43-r7000 (2.4, phy da placa 0000:01:00.0) e b43-r7000-5g (5 GHz VHT80, placa 0001:01:00.0)
K=/root/b43kit
mkdir -p /tmp/wpx /tmp/iwx; cp -a $K/b43 $K/fw /tmp/; cp -a $K/wpx/r /tmp/wpx/; cp -a $K/iwx/r /tmp/iwx/
cp $K/hostapd.conf $K/hostapd5.conf $K/wpa.conf $K/sta.sh /tmp/
export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib:/tmp/iwx/r/lib
IW=/tmp/iwx/r/usr/sbin/iw
echo -n /tmp/fw > /sys/module/firmware_class/parameters/path
wifi down; sleep 2; killall nas 2>/dev/null; rmmod wl; sleep 1
for d in 0000:01:00.0 0001:01:00.0; do echo $d > /sys/bus/pci/drivers/bcma-pci-bridge/bind 2>/dev/null; done; sleep 1
for m in compat cfg80211 mac80211 b43; do insmod /tmp/b43/$m.ko || echo "FALHA $m"; done; sleep 4
$IW reg reload; $IW reg set BR; sleep 1
for p in $(ls /sys/class/ieee80211/); do
	if $IW phy $p info | grep -q "5180.0 MHz"; then
		$IW phy $p interface add wlap5 type __ap
		start-stop-daemon -S -b -n hostapd5 -x /tmp/wpx/r/usr/sbin/hostapd -- /tmp/hostapd5.conf
	else
		$IW phy $p interface add wlap0 type __ap
		start-stop-daemon -S -b -n hostapd24 -x /tmp/wpx/r/usr/sbin/hostapd -- /tmp/hostapd.conf
	fi
done
sleep 8
$IW dev | grep -E "Interface|ssid|channel"
/tmp/iwx/r/usr/bin/iperf3 -s -D -B 192.168.1.70
