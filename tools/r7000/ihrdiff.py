#!/usr/bin/env python3
# Compara os registradores do MAC d11: /proc/wlshm do wl (unidade 1) contra
# o ihr-b43.txt do tools/r7000/ihrdump.sh.
# uso: ihrdiff.py wlshm.bin ihr-b43.txt
import struct
import sys

SHM, PMU, IHR = 0x2000, 0x100, 0x200 + 0xc00
UNIT = SHM + PMU + IHR
d = open(sys.argv[1], 'rb').read()
base = 1 * UNIT + SHM + PMU
wl = {}
for a in range(0, 0x200, 4):
    wl[a] = struct.unpack_from('<I', d, base + a)[0]
for a in range(0x400, 0x1000, 2):
    wl[a] = struct.unpack_from('<H', d, base + 0x200 + a - 0x400)[0]
b43 = {}
for l in open(sys.argv[2]):
    f = l.split()
    if len(f) == 2:
        b43[int(f[0], 16)] = int(f[1], 16)
n = 0
for a in sorted(b43):
    if a in wl and wl[a] != b43[a] and not (0x160 <= a <= 0x16c):
        print('%03x wl %08x b43 %08x' % (a, wl[a], b43[a]))
        n += 1
print(n, 'diferencas de', len(b43))
