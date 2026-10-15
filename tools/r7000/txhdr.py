#!/usr/bin/env python3
# Campos do cabecalho de TX dos pacotes despejados pelo patch 005 do wl.
# uso: txhdr.py wl-tx-boot.txt
import re
import sys

pk = []
for l in open(sys.argv[1]):
    m = re.search(r'wltrace-tx (\S+) len (\d+)', l)
    if m:
        pk.append([m.group(1), int(m.group(2)), b''])
        continue
    m = re.search(r'wltrace-tx [0-9a-f]{8}: ((?:[0-9a-f]{2} ?)+)', l)
    if m and pk:
        pk[-1][2] += bytes.fromhex(m.group(1).replace(' ', ''))
u16 = lambda b, o: b[o] | b[o + 1] << 8
for name, ln, b in pk:
    if len(b) < 0x84:
        continue
    print('%s len %3d pre %s low %04x high %04x x0a %04x ivo %02x pcl %02x flen %04x fid %04x '
          'ctl0 %04x 1a %04x 1c %04x plcp %s 24 %04x rate %04x 28 %04x fc %04x' % (
              name, ln, b[0:6].hex(), u16(b, 6), u16(b, 8), u16(b, 0x0a), b[0x0c], b[0x0d],
              u16(b, 0x0e), u16(b, 0x10), u16(b, 0x18), u16(b, 0x1a), u16(b, 0x1c),
              b[0x1e:0x24].hex(), u16(b, 0x24), u16(b, 0x26), u16(b, 0x28), u16(b, 0x80)))
