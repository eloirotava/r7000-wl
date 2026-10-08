#!/usr/bin/env python3
"""Confere, via saida do `pahole -C sk_buff`, os offsets que o nucleo
binario do wl (wlc_*/phy 7.14.164.18) usa por endereco fixo."""
import re, sys

# offsets medidos no desassembly dos objetos do DD-WRT (kernel 4.4, ARM32)
ESPERADO = {
    "next": 0x00, "prev": 0x04,
    "pktc_cb": 0x10, "ctf_pppoe_cb": 0x18, "ctfmap": 0x20,
    "bcm_sk": 0x24, "bcm_dev": 0x28,
    "cb": 0x30,
    "ctfpool": 0x70, "pktc_flags": 0x74, "ctf_ipc_txif": 0x78,
    "napt_idx": 0x7c, "napt_flags": 0x80,
    "len": 0x94, "data_len": 0x98,
    "priority": 0xb4,
    "tail": 0xdc, "end": 0xe0, "head": 0xe4, "data": 0xe8,
}

campo = re.compile(r"^\s*[^/;]*?[\s\*](\w+)(?:\[\d+\])?(?::\d+)?(?:\s+__attribute__\(\(.*?\)\))?;\s*/\*\s*(\d+)")
achado = {}
for linha in open(sys.argv[1]):
    m = campo.match(linha)
    if m and m.group(1) in ESPERADO and m.group(1) not in achado:
        achado[m.group(1)] = int(m.group(2))

ok = True
for nome, off in ESPERADO.items():
    real = achado.get(nome)
    marca = "ok" if real == off else "ERRO"
    if real != off:
        ok = False
    print(f"{marca:4} {nome:14} esperado 0x{off:03x}  real "
          + ("ausente" if real is None else f"0x{real:03x}"))
sys.exit(0 if ok else 1)
