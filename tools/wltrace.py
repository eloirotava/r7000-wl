#!/usr/bin/env python3
# decodifica o /proc/wltrace do wl (src/wltrace.c)
# uso: wltrace.py arquivo.bin [-u UNIDADE] [-r] [-s]
#   -u so uma unidade (0/1 = cada radio)   -r omite leituras   -s so resumo
import struct, sys, collections

T = {1: 'phy_w', 2: 'phy_r', 3: 'phy_mod', 4: 'phy_and', 5: 'phy_or',
     6: 'rad_w', 7: 'rad_r', 8: 'rad_mod', 9: 'rad_or', 10: 'tbl_w',
     11: 'tbl_r', 12: 'data', 13: 'delay', 14: 'pmu', 15: 'corereg',
     16: 'shm_w', 17: 'mhf', 18: 'event', 19: 'tblregs'}
EV = {1: 'suspend_mac', 2: 'enable_mac', 3: 'phyclk_fgc', 4: 'bw_set',
      5: 'switch_macfreq'}

args = sys.argv[1:]
unit = int(args[args.index('-u') + 1]) if '-u' in args else None
noreads = '-r' in args
summary = '-s' in args
d = open(args[0], 'rb').read()
recs = [struct.unpack_from('<4I', d, i) for i in range(0, len(d) - 15, 16)]

count = collections.Counter()
i = 0
while i < len(recs):
    h, a, b, c = recs[i]
    t, u = h & 0xff, (h >> 8) & 0xf
    i += 1
    data = b''
    if t in (10,):
        n = (c & 0xffffff) * ((c >> 24) // 8)
        while i < len(recs) and recs[i][0] & 0xff == 12 and len(data) < n:
            data += struct.pack('<3I', *recs[i][1:])
            i += 1
        data = data[:n]
    count[T.get(t, t)] += 1
    if summary or (unit is not None and u != unit and u != 0xf and t < 13):
        continue
    if noreads and t in (2, 7, 11):
        continue
    name = T.get(t, '?%d' % t)
    if t in (1, 2, 6, 7):
        s = '%04x = %04x' % (a, b)
    elif t in (3, 8):
        s = '%04x mask %04x val %04x' % (a, b, c)
    elif t in (4, 5, 9):
        s = '%04x %s %04x' % (a, '&' if t == 4 else '|', b)
    elif t in (10, 11):
        w = c >> 24
        s = 'id %#x off %#x len %d w%d' % (a, b, c & 0xffffff, w)
        if data:
            fmt = {8: 'B', 16: 'H', 32: 'I'}.get(w, 'B')
            vals = struct.unpack('<%d%s' % (len(data) // struct.calcsize(fmt), fmt), data)
            s += ' [' + ' '.join('%x' % v for v in vals[:16]) + (' ...]' if len(vals) > 16 else ']')
    elif t == 13:
        s = '%d us' % a
    elif t == 14:
        s = 'reg %d mask %#x val %#x' % (a, b, c)
    elif t == 15:
        s = 'core %d off %#x mask %#x val %#x' % (a >> 16, a & 0xffff, b, c)
    elif t == 16:
        s = '%#x = %#x' % (a, b)
    elif t == 17:
        s = 'idx %d mask %#x val %#x' % (a, b, c)
    elif t == 19:
        s = 'args %04x %04x pilha %04x %04x %04x' % (a >> 16, a & 0xffff, b >> 16, b & 0xffff, c)
    elif t == 18:
        s = EV.get(a, a) + ('' if a in (1, 2) else ' %d' % b)
    else:
        s = '%x %x %x' % (a, b, c)
    print('%6d u%x %-8s %s' % (h >> 12, u, name, s))

print('# %d registros: %s' % (len(recs), dict(count)), file=sys.stderr)
