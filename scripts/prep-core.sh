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

# WLTRACE=1: nos objetos do PHY, as chamadas aos acessores (que vem de
# outro objeto) passam por wltrace_* (src/wltrace.c).  Definicoes intactas.
rm -f "$(dirname "$out")/wltrace"
if [ "$WLTRACE" = 1 ]; then
	traced='phy_reg_write phy_reg_read phy_reg_mod phy_reg_and phy_reg_or
		write_radio_reg read_radio_reg mod_radio_reg or_radio_reg
		wlc_phy_write_table_ext wlc_phy_read_table_ext osl_delay
		si_pmu_regcontrol si_corereg wlapi_bmac_write_shm wlapi_bmac_mhf
		wlapi_suspend_mac_and_wait wlapi_enable_mac wlapi_bmac_phyclk_fgc
		wlapi_bmac_bw_set wlapi_switch_macfreq'
	mkdir "$tmp/phy"
	new=""
	for o in $objs; do
		case "$o" in */wl/phy/*) ;; *) new="$new $o"; continue ;; esac
		und=$("${CROSS_COMPILE}nm" -u "$o" | awk '{print $2}')
		args=""
		for s in $traced; do
			echo "$und" | grep -qx "$s" && args="$args --redefine-sym $s=wltrace_$s"
		done
		c="$tmp/phy/$(basename "$o")"
		"${CROSS_COMPILE}objcopy" $args "$o" "$c"
		[ -n "$args" ] && echo "wltrace: $(basename "$o"):$(echo $args | sed 's/--redefine-sym //g; s/=wltrace_[a-z_0-9]*//g')"
		new="$new $c"
	done
	objs=$new
	touch "$(dirname "$out")/wltrace"
fi

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
