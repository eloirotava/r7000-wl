#!/usr/bin/env python3
# Gera b43/src/phy_ac_replay.c: o init do AC-PHY/radio 2069 que o wl faz,
# gravado pelo wltrace (texto de tools/wltrace.py), para o b43 reproduzir.
#
# uso: gen-replay.py <rastro.txt> <unidade pi> <unidade shim> <seq ini> <seq fim> <saida.c>
#
# Entra: escritas/modificacoes do PHY e do radio, tabelas (com os dados),
# delays, regcontrol da PMU e escritas na SHM.  Fica de fora: leituras (polls
# e read-modify-write ja aparecem como mod), eventos do MAC (o b43 ja inicia
# o PHY com o MAC suspenso) e mhf.
import sys, struct

src, upi, ush, s0, s1, out = sys.argv[1:7]
s0, s1 = int(s0), int(s1)

OP = {'phy_w': 1, 'phy_mod': 2, 'phy_and': 3, 'phy_or': 4,
      'rad_w': 5, 'rad_mod': 6, 'rad_or': 7, 'tbl_w': 8, 'delay': 9,
      'pmu': 10, 'shm_w': 11}
ops, tbls, blob, pmus = [], [], [], []
names = {}

lines = open(src).read().splitlines()
i = 0
while i < len(lines):
    f = lines[i].split()
    i += 1
    if len(f) < 4:
        continue
    seq, unit, t = int(f[0]), f[1], f[2]
    if seq < s0 or seq > s1:
        continue
    if unit not in ('u' + upi, 'u' + ush, 'ue'):
        continue
    if t in ('phy_w', 'rad_w'):
        ops.append((OP[t], int(f[3], 16), 0, int(f[5], 16)))
    elif t in ('phy_mod', 'rad_mod'):
        ops.append((OP[t], int(f[3], 16), int(f[5], 16), int(f[7], 16)))
    elif t in ('phy_and', 'phy_or', 'rad_or'):
        ops.append((OP[t], int(f[3], 16), 0, int(f[5], 16)))
    elif t == 'delay':
        us = int(f[3])
        while us > 0:
            ops.append((OP[t], 0, 0, min(us, 0xffff)))
            us -= 0xffff
    elif t == 'pmu' and unit == 'ue':
        # 'reg N mask M val V'
        ops.append((OP[t], int(f[4]), 0, len(pmus)))
        pmus.append((int(f[6], 16), int(f[8], 16)))
    elif t == 'shm_w':
        ops.append((OP[t], int(f[3], 16), 0, int(f[5], 16)))
    elif t == 'tbl_w':
        # 'id 0x.. off 0x.. len N wW [...]' -> dados completos vem do .bin;
        # aqui so o cabecalho; os dados sao lidos do binario (abaixo)
        tid, off, n, w = int(f[4], 16), int(f[6], 16), int(f[8]), int(f[9][1:])
        ops.append((OP[t], tid, off, len(tbls)))
        tbls.append([seq, w, n, None])

# dados das tabelas: do .bin (o texto so mostra 16 valores)
binf = src.rsplit('.', 1)[0] + '.bin'
d = open(binf, 'rb').read()
recs = [struct.unpack_from('<4I', d, k) for k in range(0, len(d) - 15, 16)]
byseq = {}
k = 0
while k < len(recs):
    h, a, b, c = recs[k]
    if h & 0xff == 10:
        n = (c & 0xffffff) * ((c >> 24) // 8)
        data = b''
        j = k + 1
        while j < len(recs) and recs[j][0] & 0xff == 12 and len(data) < n:
            data += struct.pack('<3I', *recs[j][1:])
            j += 1
        byseq[h >> 12] = data[:n]
        k = j
    else:
        k += 1
for t in tbls:
    seq, w, n, _ = t
    data = byseq[seq & 0xfffff] if seq in byseq else byseq.get(seq)
    if data is None:
        sys.exit('tabela seq %d sem dados' % seq)
    if w == 48:
        # 3 x u16 por entrada (bits 15:0, 31:16, 47:32)
        vals = list(struct.unpack('<%dH' % (3 * n), data))
    else:
        fmt = {8: 'B', 16: 'H', 32: 'I'}[w]
        vals = list(struct.unpack('<%d%s' % (n, fmt), data))
    t[3] = len(blob)
    blob += vals

o = ['// SPDX-License-Identifier: GPL-2.0-or-later',
     '/*',
     ' * Broadcom B43 wireless driver',
     ' * Init do AC-PHY/radio 2069 rev 4 (BCM4360 B1) reproduzido do wl:',
     ' * gravado com o wltrace no Netgear R7000 (seq %d-%d, unidade %s).' % (s0, s1, upi),
     ' * Gerado por tools/gen-replay.py; nao editar a mao.',
     ' */',
     '',
     '#include "b43.h"',
     '#include "phy_ac_replay.h"',
     '',
     'const struct b43_ac_replay_op b43_ac_replay_ops[] = {']
for t, a, m, v in ops:
    o.append('\t{ %2d, 0x%04x, 0x%04x, 0x%04x },' % (t, a, m, v))
o.append('};')
o.append('const unsigned int b43_ac_replay_nops = ARRAY_SIZE(b43_ac_replay_ops);')
o.append('')
o.append('const struct b43_ac_replay_tbl b43_ac_replay_tbls[] = {')
for seq, w, n, start in tbls:
    o.append('\t{ %2d, %4d, %6d },' % (w, n, start))
o.append('};')
o.append('')
o.append('const struct b43_ac_replay_pmu b43_ac_replay_pmus[] = {')
for m, v in pmus:
    o.append('\t{ 0x%08x, 0x%08x },' % (m, v))
o.append('};')
o.append('')
o.append('const u32 b43_ac_replay_data[] = {')
for j in range(0, len(blob), 8):
    o.append('\t' + ' '.join('0x%x,' % x for x in blob[j:j + 8]))
o.append('};')
open(out, 'w').write('\n'.join(o) + '\n')
from collections import Counter
c = Counter(t for t, *_ in ops)
inv = {v: k for k, v in OP.items()}
print('%d ops %s, %d tabelas, %d valores de tabela' %
      (len(ops), {inv[k]: v for k, v in sorted(c.items())}, len(tbls), len(blob)))
print('delay total %.1f ms' % (sum(v for t, a, m, v in ops if t == 9) / 1000))
