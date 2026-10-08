# r7000-wl

OpenWrt (25.12, kernel 6.12) no Netgear R7000 (BCM4709 + 2x BCM4360) com
o driver `wl` da Broadcom.

Ideia: o nucleo do `wl` (`wlc_*`/`phy`) so depende do kernel atraves do
layout de `struct sk_buff` e de umas poucas funcoes; a camada de SO
(`wl_linux.c`) e reescrita aqui.  O kernel recebe um patch que deixa o
`sk_buff` com os offsets que o nucleo binario espera.

- `patches/990-skbuff-bcm-wl-abi.patch` — layout do `sk_buff` (ARM32).
- `scripts/check-skb-abi.py` — confere os offsets com `pahole`; o build
  falha se algum nao bater.
- `.github/workflows/build.yml` — imagem do R7000 + SDK.

Os objetos binarios da Broadcom **nao** estao neste repositorio.

## Offsets exigidos

| campo | offset | | campo | offset |
|---|---|---|---|---|
| next / prev | 0x00 / 0x04 | | pktc_flags | 0x74 |
| pktc_cb | 0x10 | | len / data_len | 0x94 / 0x98 |
| cb[48] | 0x30 | | priority | 0xb4 |
| tail / end | 0xdc / 0xe0 | | head / data | 0xe4 / 0xe8 |
