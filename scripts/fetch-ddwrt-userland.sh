#!/bin/sh
# Extrai do firmware oficial do DD-WRT para o R7000 o espaco de usuario
# Broadcom que o OpenWrt nao tem: nas (autenticador WPA), wl (utilitario) e
# as bibliotecas de que o nas depende. Nao versionamos esses binarios.
# Uso: fetch-ddwrt-userland.sh <raiz do files/>
set -e
URL=https://download1.dd-wrt.com/dd-wrtv2/downloads/betas/2026/03-18-2026-r64079/netgear-r7000/factory-to-dd-wrt.chk
SHA256=ca06908ddf5d7b9b5f4a611b382ae1d026d124ea1642aae592eff20f378ab1ef

dst=$1
[ -n "$dst" ] || { echo "uso: $0 <destino>" >&2; exit 1; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

curl -fsSL --retry 5 -o "$tmp/fw.chk" "$URL"
echo "$SHA256  $tmp/fw.chk" | sha256sum -c -

# chk = cabecalho Netgear + TRX; o squashfs comeca no 2o offset do TRX
off=$(python3 -I - "$tmp/fw.chk" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
t = d.find(b'HDR0')
offs = struct.unpack_from('<3I', d, t + 16)
print(t + offs[1])
EOF
)
dd if="$tmp/fw.chk" of="$tmp/root.sqfs" bs=4096 iflag=skip_bytes skip="$off" status=none
unsquashfs -q -d "$tmp/fs" "$tmp/root.sqfs" usr/sbin/nas usr/sbin/wl \
	usr/lib/libshutils.so usr/lib/libutils.so usr/lib/libwireless.so

mkdir -p "$dst/usr/sbin" "$dst/usr/lib/ddwrt"
install -m 755 "$tmp/fs/usr/sbin/nas" "$tmp/fs/usr/sbin/wl" "$dst/usr/sbin/"
install -m 644 "$tmp/fs/usr/lib/"lib*.so "$dst/usr/lib/ddwrt/"
ls -la "$dst/usr/sbin" "$dst/usr/lib/ddwrt"
