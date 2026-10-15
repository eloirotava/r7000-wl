#!/usr/bin/env python3
# Gera "0x1 0xOFF 0x0 0xVAL" para toda a SHM compartilhada da unidade 1
# do /proc/wlshm do wl (menos macstat 0xe0-0x15f e canal 0xa0), para o
# debugfs shm16write do b43.  uso: mkshmall.py wlshm.bin > shmall.txt
import struct
import sys

d = open(sys.argv[1], 'rb').read()
unit = len(d) // 2
w = struct.unpack_from('<4096H', d, unit)
for i, v in enumerate(w):
    o = i * 2
    if 0xe0 <= o < 0x160 or o == 0xa0:
        continue
    print('0x1 0x%x 0x0 0x%x' % (o, v))
