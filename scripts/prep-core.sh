#!/bin/sh
# Junta os objetos binarios do nucleo do wl num unico wl_core.o,
# renomeando os simbolos do kernel 4.4 que mudaram no 6.12.
# Uso: prep-core.sh <arvore brcm/arm> <saida wl_core.o>
set -e
brcm=$1; out=$2
: "${CROSS_COMPILE:=arm-linux-gnueabi-}"

# fora do link, como no wl.ko do DD-WRT (puxam simbolos inexistentes)
skip='wl_linux.o wl_iw.o wlc_modesw.o wlc_wmf.o'

objs=""
for o in "$brcm"/wl/sys/*.o "$brcm"/wl/phy/*.o \
	"$brcm"/wl/clm/src/*.o "$brcm"/wl/olpc/src/*.o "$brcm"/wl/ppr/src/*.o \
	"$brcm"/bcmcrypto/*.o \
	"$brcm"/shared/qmath.o "$brcm"/shared/bcm_mpool.o \
	"$brcm"/shared/bcm_notif.o "$brcm"/shared/bcmwpa.o; do
	case " $skip " in *" $(basename "$o") "*) continue ;; esac
	objs="$objs $o"
done

tmp=$(mktemp -d)
"${CROSS_COMPILE}ld" -r -o "$tmp/core.o" $objs
"${CROSS_COMPILE}objcopy" \
	--redefine-sym kmalloc_caches=wl_kmalloc_caches \
	--redefine-sym kmem_cache_alloc=wl_kmem_cache_alloc \
	--redefine-sym __kmalloc=wl___kmalloc \
	"$tmp/core.o" "$out"
rm -rf "$tmp"
echo "$(echo $objs | wc -w) objetos -> $out"
"${CROSS_COMPILE}nm" -u "$out" | awk '{print $2}' | sort > "$out.undef"
echo "$(wc -l < "$out.undef") simbolos indefinidos (lista em $out.undef)"
