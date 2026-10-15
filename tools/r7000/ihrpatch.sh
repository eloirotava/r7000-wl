#!/bin/sh
# No R7000 com o b43 no ar: grava os valores do wl (/tmp/ihrpatch.txt,
# "0xREG 0x0 0xVAL") nos registradores do MAC e repete um probe
# direcionado, medindo txallfrm e o anel TX3.  Log em /root/ihrpatch.log
D=/sys/kernel/debug/b43/phy0
export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib
IW=/tmp/iwx/r/usr/sbin/iw
ms() {
	echo "0x1 0xe0" > $D/shm16read; t=$(cat $D/shm16read)
	echo 0x2d0 > $D/mmio32read; s=$(cat $D/mmio32read)
	echo "txallfrm=$t tx3st0=$s"
}
exec > /root/ihrpatch.log 2>&1
echo antes; ms
while read l; do echo "$l" > $D/mmio16write; done < /tmp/ihrpatch.txt
sync
echo gravado; ms
$IW dev wlb0 scan trigger freq 5220 ssid auxiliar
sleep 6
echo depois; ms
$IW dev wlb0 scan dump | grep -E '^BSS|SSID|Probe Response' | head -20
echo FIM
