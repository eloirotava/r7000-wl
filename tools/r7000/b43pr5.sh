#!/bin/sh
# roda no R7000 (start-stop-daemon -b): tira o wl, entrega o radio 5G
# (0001:01:00.0) ao bcma-pci-bridge, carrega o b43 de /tmp/b43, cria wlb0
# e sobe.  Log em /tmp/b43.log; o wifi volta com reboot (testguard armado).
exec > /root/b43.log 2>&1
( while :; do dmesg | tail -40 > /root/b43dm.txt; sync; sleep 1; done ) &
mount -t debugfs none /sys/kernel/debug 2>/dev/null
regs() {
	D=/sys/kernel/debug/b43/phy0
	for r in 0x120 0x128 0x12c 0x20 0x24 0x220 0x224 0x228 0x230 0x234; do
		echo $r > $D/mmio32read; echo "mmio32 $r $(cat $D/mmio32read)"
	done
	for r in 0x40 0x80 0x82 0xe0 0xe2 0xe4 0xe6 0xe8 0xea 0xec 0xee 0xf0 0xf2 0xf4 0xf6 0xf8 0xfa 0xfc 0xfe 0x100 0x102 0x104 0x106 0x108 0x10a 0x10c 0x10e 0x110 0x112 0x114 0x116 0x118 0x11a 0x11c 0x11e 0x120 0x122 0x124 0x126 0x128 0x12a 0x12c 0x12e 0x130 0x132 0x134 0x136 0x138 0x13a 0x13c 0x13e; do
		echo "0x1 $r" > $D/shm16read; echo "shm $r $(cat $D/shm16read)"
	done
	grep -E 'b43|wlb0' /proc/interrupts
}
set -x
D=0001:01:00.0
export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib
IW=/tmp/iwx/r/usr/sbin/iw
if [ ! -x $IW ]; then
	apk update >/dev/null 2>&1
	mkdir -p /tmp/iwx/r; cd /tmp/iwx
	apk fetch iw libnl-tiny1 >/dev/null 2>&1
	for p in *.apk; do apk extract --allow-untrusted --destination /tmp/iwx/r "$p"; done
	cd /
fi
echo -n /tmp/fw > /sys/module/firmware_class/parameters/path
wifi down; sleep 2; killall nas; sleep 1
rmmod wl
sleep 1
echo $D > /sys/bus/pci/drivers/bcma-pci-bridge/bind
sleep 1
for m in compat cfg80211 mac80211; do insmod /tmp/b43/$m.ko || echo "FALHA $m"; done
insmod /tmp/b43/b43.ko verbose=3 $(cat /tmp/b43args 2>/dev/null) || echo "FALHA b43"
sleep 3
P=$(ls /sys/class/ieee80211/ | head -1)
$IW phy $P interface add wlb0 type managed
ip link set wlb0 up
sleep 8
dmesg | grep -E 'b43|phy[0-9]|wlb0' | grep -vE ' from |Exception|^\[ *[0-9.]+\] [0-9a-f]{4}:' | tail -60
ip -br link | grep wlb0
regs
$IW dev wlb0 scan trigger freq 5220 ssid b43-teste 2>&1 | head -3
sleep 6
$IW dev wlb0 scan dump 2>&1 | grep -E '^BSS|SSID|freq|signal|Probe Response|Beacon frame' | head -40
$IW dev wlb0 connect b43-teste
sleep 10
$IW dev wlb0 link
$IW dev wlb0 station dump | grep -E "Station|signal|bitrate|packets"
dmesg | grep -E "wlb0: (auth|assoc|associated)" | tail -8
dmesg | grep -E 'b43' | tail -15
regs
a=0; while [ $a -lt 8192 ]; do echo "0x1 $(printf 0x%x $a)" > /sys/kernel/debug/b43/phy0/shm16read; cat /sys/kernel/debug/b43/phy0/shm16read; a=$((a+2)); done > /root/b43shm.txt
sync
echo FIM
