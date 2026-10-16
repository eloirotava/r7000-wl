#!/bin/sh
# Roda NO R7000. dump24.sh wl|b43 <saida>: le registradores do PHY do radio
# 2.4 (0000:01:00.0) no canal 6. wl: wl0 up so para leitura (sem SSID) e
# down no fim; b43: pela janela 0x3fc/0x3fe do debugfs.
M=$1; O=$2
REGS="$(i=0; while [ $i -lt 512 ]; do printf '0x%03x ' $i; i=$((i+1)); done)"
REGS="$REGS $(for b in 0x700 0x900 0xb00; do i=0; while [ $i -lt 96 ]; do printf '0x%03x ' $((b+i)); i=$((i+1)); done; done)"
REGS="$REGS $(i=0; while [ $i -lt 64 ]; do printf '0x%03x ' $((0x400+i)); i=$((i+1)); done)"
REGS="$REGS $(i=0; while [ $i -lt 128 ]; do printf '0x%04x ' $((0x1700+i)); i=$((i+1)); done)"
: > $O
if [ "$M" = wl ]; then
	wl -i wl0 down; wl -i wl0 chanspec 6; wl -i wl0 up; sleep 3
	for r in $REGS; do echo "$r $(wl -i wl0 phyreg $r 2>/dev/null)" >> $O; done
	wl -i wl0 down
else
	D=/sys/kernel/debug/b43/phy0
	for r in $REGS; do echo "0x3fc 0x0 $r" > $D/mmio16write; echo 0x3fe > $D/mmio16read; echo "$r $(cat $D/mmio16read)" >> $O; done
fi
wc -l < $O
