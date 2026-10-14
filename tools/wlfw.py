#!/usr/bin/env python3
# extrai ucode/initvals de um wl.ko (ELF32 LE) no formato de firmware do b43
import struct, sys, os
f, out = sys.argv[1], sys.argv[2]
d = open(f, 'rb').read()
shoff, = struct.unpack_from('<I', d, 0x20)
shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
sh = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
symtab = next(s for s in sh if s[1] == 2)
strtab = sh[symtab[6]]
def name(off, base): e = d.index(b'\0', base + off); return d[base + off:e].decode()
syms = {}
for i in range(symtab[5] // 16):
    st_name, val, size, info, oth, shndx = struct.unpack_from('<IIIBBH', d, symtab[4] + i * 16)
    if shndx and shndx < shnum and size:
        syms[name(st_name, strtab[4])] = (sh[shndx][4] + val, size)
def blob(n): o, s = syms[n]; return d[o:o + s]
def hdr(t, n): return struct.pack('>BBxxI', ord(t), 1, n)
def ucode(sym, fn):
    b = blob(sym); w = struct.unpack('<%dI' % (len(b) // 4), b)
    # o b43 exige no cabecalho do ucode o tamanho exato dos dados
    open(os.path.join(out, fn), 'wb').write(hdr('u', len(w) * 4) + struct.pack('>%dI' % len(w), *w))
def iv(sym, fn):
    b = blob(sym); o = bytearray(); n = 0
    for i in range(0, len(b), 8):
        addr, size, val = struct.unpack_from('<HHI', b, i)
        if addr == 0xffff: break
        if size == 4: o += struct.pack('>HI', addr | 0x8000, val)
        elif size == 2: o += struct.pack('>HH', addr, val)
        else: sys.exit('size %d em %s' % (size, sym))
        n += 1
    open(os.path.join(out, fn), 'wb').write(hdr('i', n) + o)
    print(fn, n, 'valores')
ucode('d11ucode42', 'ucode42.fw')
iv('d11ac1initvals42', 'ac1initvals42.fw')
iv('d11ac1bsinitvals42', 'ac1bsinitvals42.fw')
