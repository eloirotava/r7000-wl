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

## Estado (2026-10-08)

Rodando no R7000 do lab: OpenWrt 25.12 (kernel 6.12.112 com o patch do
`sk_buff`), driver `wl` 7.14 nos dois BCM4360, AP WPA2-PSK/AES em 2.4 GHz
(canal 6) e 5 GHz (36, 40 MHz) na `br-lan`, sobe sozinho no boot.

- `package/kernel/wl-r7000` — modulo: nucleo binario do DD-WRT + `wl_linux.c`
  proprio + `shared/` da Broadcom compilado do fonte.
- `files/etc/init.d/wl-r7000` + `files/etc/config/wl-r7000` — servico:
  carrega o `wl`, configura os radios (UCI), bridge e um `nas` por radio.
  Protecao contra boot-loop: depois de 3 boots sem estabilizar (10 min)
  o `wl` nao carrega (`/etc/wl-r7000.fails`).
- `tools/libnvram-shim` — `libnvram.so` substituto para rodar o `nas` do
  DD-WRT (e as libs dele) no OpenWrt.
- `files/usr/sbin/netwatch` — reboot apos 5 min sem internet.

Binarios do DD-WRT que precisam estar no roteador (nao vao no repo):
`/usr/sbin/wl`, `/usr/sbin/nas`, `/usr/lib/ddwrt/lib{shutils,utils,wireless}.so`.

Pendente: 80 MHz em 5 GHz (o CLM do BR neste driver so oferece 40),
o aviso do FORTIFY em `bcm_write_tlv` e medir vazao com um cliente bom.
