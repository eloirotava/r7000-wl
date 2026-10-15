#!/bin/sh
# No R7000 com o b43 no ar: registradores do MAC d11 pelo debugfs.
# 0x000-0x1ff em 32 bits (sem 0x160-0x16c: objaddr/objdata) e IHR
# 0x400-0xffe em 16 bits.  Pula DMA (0x200-0x3ff) e o acesso indireto
# do PHY/radio.  Saida: "offset valor" em /root/ihr-b43.txt
D=/sys/kernel/debug/b43/phy0
a=0
while [ $a -lt 512 ]; do
	if [ $a -lt 352 ] || [ $a -gt 364 ]; then
		echo "$(printf 0x%x $a)" > $D/mmio32read
		echo "$(printf %03x $a) $(cat $D/mmio32read)"
	fi
	a=$((a + 4))
done > /root/ihr-b43.txt
a=1024
while [ $a -lt 4096 ]; do
	echo "$(printf 0x%x $a)" > $D/mmio16read
	echo "$(printf %03x $a) $(cat $D/mmio16read)"
	a=$((a + 2))
done >> /root/ihr-b43.txt
sync
wc -l /root/ihr-b43.txt
