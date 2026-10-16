#!/bin/sh
# Roda NO R7000 com o b43 no ar: dumprad.sh <lista> <saida>. Le os
# registradores do radio (enderecos da lista, 1a coluna) pela janela
# RADIO24 do core (MMIO 0x3d8 controle, 0x3da dados) via debugfs.
D=/sys/kernel/debug/b43/phy0
: > $2
while read r v; do
	echo "0x3d8 0x0 $r" > $D/mmio16write
	echo 0x3da > $D/mmio16read
	echo "$r $(cat $D/mmio16read)" >> $2
done < $1
wc -l < $2
