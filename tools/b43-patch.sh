#!/bin/sh
# Regenera b43/patches/brcm/950-b43-ac-phy-r7000.patch a partir de b43/src
# (fonte editavel do AC-PHY) contra o backports da versao do SDK.
# Uso: tools/b43-patch.sh [diretorio com backports-6.18.55 extraido]
set -e
V=6.18.55
here=$(cd "$(dirname "$0")/.." && pwd)
bp=$1
if [ -z "$bp" ]; then
	bp=$(mktemp -d)
	curl -sL https://github.com/openwrt/backports/releases/download/backports-v$V/backports-$V.tar.zst |
		tar --zstd -x -C "$bp"
fi
up=$bp/backports-$V/drivers/net/wireless/broadcom/b43
w=$(mktemp -d)
mkdir -p "$w/a/drivers/net/wireless/broadcom/b43" "$w/b/drivers/net/wireless/broadcom/b43"
for f in Kconfig $(cd "$here/b43/src" && ls); do
	cp "$up/$f" "$w/a/drivers/net/wireless/broadcom/b43/" 2>/dev/null ||
		: > "$w/a/drivers/net/wireless/broadcom/b43/$f"
done
cp "$up/Kconfig" "$here"/b43/src/* "$w/b/drivers/net/wireless/broadcom/b43/"
sed -i -e 's/depends on B43 && B43_BCMA && BROKEN/depends on B43 \&\& B43_BCMA/' \
	-e '/^\tdepends on CORDIC$/d' \
	"$w/b/drivers/net/wireless/broadcom/b43/Kconfig"
out=$here/b43/patches/brcm/950-b43-ac-phy-r7000.patch
(cd "$w" && diff -ruN a b) > "$out" || true
rm -rf "$w"
echo "$(grep -c '^+++' "$out") arquivos em $out"
