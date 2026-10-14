#!/usr/bin/env python3
# Gera b43/src/radio_2069.c a partir da chan_tuning_2069rev4 do wlc_phytbl_ac.o
# (arvore Broadcom do DD-WRT, scripts/fetch-brcm.sh).
#
# Formato da entrada (116 bytes, 77 canais): canal, freq (MHz), 50 valores do
# radio 2069 e 6 do PHY (BW1A..BW6, 0x371-0x376).  A ordem dos registradores
# do radio saiu do wltrace: o wl escreve os 50 valores em sequencia, e as
# sintonias gravadas no boot (canais 36 e 108) batem com a tabela.
#
# uso: gen-chantab.py <wlc_phytbl_ac.o> <saida.c>
import struct, sys

RADIO_REGS = [
    0x08e0, 0x08e1, 0x08dd, 0x08dc, 0x08e6, 0x08e7, 0x08c4, 0x08c5, 0x08e5,
    0x08eb, 0x08d6, 0x0113, 0x08db, 0x08da, 0x08d7, 0x0885, 0x0886, 0x0887,
    0x08d9, 0x08d8, 0x08c9, 0x08ca, 0x08cc, 0x08c7, 0x08c8, 0x0892, 0x0894,
    0x0895, 0x0896, 0x0897, 0x0899, 0x089a, 0x089b, 0x089c, 0x0112, 0x0629,
    0x065b, 0x065e, 0x0668, 0x011a, 0x011b, 0x0719, 0x0630, 0x065c, 0x0662,
    0x066d, 0x0893, 0x0145, 0x0146, 0x0723,
]
assert len(RADIO_REGS) == 50

obj, out = sys.argv[1], sys.argv[2]
d = open(obj, 'rb').read()
shoff, = struct.unpack_from('<I', d, 0x20)
shentsize, shnum, _ = struct.unpack_from('<HHH', d, 0x2e)
sh = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
symtab = next(s for s in sh if s[1] == 2)
strtab = sh[symtab[6]]
def nm(off):
    b = strtab[4] + off
    return d[b:d.index(b'\0', b)].decode()
blob = None
for i in range(symtab[5] // 16):
    st_name, val, size, info, oth, shndx = struct.unpack_from('<IIIBBH', d, symtab[4] + i * 16)
    if nm(st_name) == 'chan_tuning_2069rev4':
        blob = d[sh[shndx][4] + val: sh[shndx][4] + val + size]
assert blob and len(blob) % 116 == 0, 'chan_tuning_2069rev4 nao achada'

o = []
o.append('// SPDX-License-Identifier: GPL-2.0-or-later')
o.append('/*')
o.append(' * Broadcom B43 wireless driver')
o.append(' * Tabela de canais do radio 2069 rev 4 (BCM4360 B1, Netgear R7000).')
o.append(' * Gerado por tools/gen-chantab.py; nao editar a mao.')
o.append(' */')
o.append('')
o.append('#include "b43.h"')
o.append('#include "radio_2069.h"')
o.append('')
o.append('const u16 b43_radio_2069_chan_regs[B43_RADIO_2069_CHAN_REGS] = {')
for i in range(0, 50, 8):
    o.append('\t' + ' '.join('0x%04x,' % r for r in RADIO_REGS[i:i + 8]))
o.append('};')
o.append('')
o.append('const struct b43_radio_2069_channeltab_e b43_radio_2069_r4_chantab[] = {')
for i in range(0, len(blob), 116):
    v = struct.unpack_from('<58H', blob, i)
    o.append('\t{ .channel = %d, .freq = %d,' % (v[0], v[1]))
    o.append('\t  .radio = {')
    for j in range(0, 50, 10):
        o.append('\t\t' + ' '.join('0x%04x,' % x for x in v[2 + j:2 + j + 10]))
    o.append('\t  },')
    o.append('\t  .bw = { ' + ' '.join('0x%04x,' % x for x in v[52:58]) + ' },')
    o.append('\t},')
o.append('};')
o.append('')
o.append('const unsigned int b43_radio_2069_r4_chantab_size =')
o.append('\tARRAY_SIZE(b43_radio_2069_r4_chantab);')
open(out, 'w').write('\n'.join(o) + '\n')
print('%d canais -> %s' % (len(blob) // 116, out))
