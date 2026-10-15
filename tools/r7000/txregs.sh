#!/bin/sh
# No R7000 com o b43 no ar: estado dos aneis de TX (64 bits: base 0x200 +
# 0x40*n; ctl, ptr, lo, hi, st0, st1), MACCTL/IRQ e mensagens de TX.
D=/sys/kernel/debug/b43/phy0
for b in 0x200 0x240 0x280 0x2c0 0x300; do
	l="tx $b:"
	for o in 0 4 8 12 16 20; do
		echo "$(printf 0x%x $((b + o)))" > $D/mmio32read
		l="$l $(cat $D/mmio32read)"
	done
	echo "$l"
done
for r in 0x120 0x128 0x12c; do
	echo $r > $D/mmio32read; echo "$r $(cat $D/mmio32read)"
done
cat $D/txstat 2>/dev/null | tail -5
dmesg | grep -iE 'b43.*(tx|xmit|queue|fifo|stop)' | grep -v canal | tail -15
