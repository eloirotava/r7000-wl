#!/usr/bin/env python3
# Em que funcao (simbolo .text) cai cada endereco de um .o do wl.
# uso: funcat.py <objdump> <arquivo.o> <hex> [hex...]
import subprocess
import sys

objdump, obj = sys.argv[1], sys.argv[2]
out = subprocess.run(objdump.split() + ['-t', obj], capture_output=True, text=True).stdout
syms = []
for l in out.splitlines():
    f = l.split()
    if len(f) >= 6 and f[-3] == '.text' or (len(f) >= 5 and '.text' in l and ' F ' in l):
        try:
            syms.append((int(f[0], 16), int(f[-2], 16), f[-1]))
        except ValueError:
            pass
syms.sort()
for h in sys.argv[3:]:
    a = int(h, 16)
    hit = [s for s in syms if s[0] <= a < s[0] + max(s[1], 1)]
    print(h, hit[-1][2] if hit else '?')
