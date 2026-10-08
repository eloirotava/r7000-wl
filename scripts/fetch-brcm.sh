#!/bin/sh
# Baixa do espelho do DD-WRT a arvore Broadcom (headers, shared/ e os
# objetos do nucleo do wl 7.14.164.18 para ARM) num commit fixo.
# Uso: fetch-brcm.sh <destino>
set -e
DDWRT_REPO=https://github.com/mirror/dd-wrt
DDWRT_COMMIT=53bb3ad4669d10c9af4a210b4e6b427035c688e7
SUB=src/linux/universal/linux-4.4/brcm/arm

dst=$1
[ -n "$dst" ] || { echo "uso: $0 <destino>" >&2; exit 1; }
tmp=$(mktemp -d)
git -C "$tmp" init -q
git -C "$tmp" remote add origin "$DDWRT_REPO"
git -C "$tmp" sparse-checkout set --no-cone "/$SUB/"
git -C "$tmp" fetch -q --depth 1 --filter=blob:none origin "$DDWRT_COMMIT"
git -C "$tmp" checkout -q FETCH_HEAD
rm -rf "$dst"
mkdir -p "$(dirname "$dst")"
mv "$tmp/$SUB" "$dst"
rm -rf "$tmp"

# correcoes da arvore Broadcom para o 6.x (package/kernel/wl-r7000/brcm-patches)
here=$(cd "$(dirname "$0")/.." && pwd)
for p in "$here"/package/kernel/wl-r7000/brcm-patches/*.patch; do
	[ -e "$p" ] || continue
	echo "aplicando $(basename "$p")"
	patch -d "$dst" -p1 --no-backup-if-mismatch < "$p"
done
du -sh "$dst"
