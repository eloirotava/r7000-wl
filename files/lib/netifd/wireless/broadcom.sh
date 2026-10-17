#!/bin/sh
# Handler do netifd para o wl da Broadcom (wifi-device type 'broadcom'):
# configura o radio a partir do /etc/config/wireless, entrega a interface
# ao netifd (que a poe na bridge da rede) e sobe um nas (autenticador WPA
# do DD-WRT) como processo do netifd.  O wl.ko e carregado antes, pelo
# /etc/init.d/wl-r7000 (protecao contra boot-loop).

. /lib/netifd/netifd-wireless.sh

init_wireless_driver "$@"

drv_broadcom_init_device_config() {
	config_add_string ifname country
	config_add_int txpower
}

drv_broadcom_init_iface_config() {
	config_add_boolean hidden
}

drv_broadcom_init_vlan_config() {
	return
}

drv_broadcom_init_station_config() {
	return
}

drv_broadcom_cleanup() {
	return
}

# o netifd 25.12 nao mata os processos do handler (wireless_process_kill_all
# nao existe mais no wireless-device.uc): o nas de cada radio fica num pidfile
# o nas do DD-WRT ignora SIGTERM: sobrava o antigo junto do novo e os dois
# disputavam o 4-way (cliente WPA2 nao completava apos wifi reload). Mata
# por pidfile e tambem qualquer nas perdido no mesmo ifname, com KILL no fim
nas_stop() {
	local pf=/var/run/wl-nas.$1.pid pids p n=0
	pids="$(cat "$pf" 2>/dev/null)"
	for p in /proc/[0-9]*; do
		tr '\0' ' ' < $p/cmdline 2>/dev/null | grep -q "^/usr/sbin/nas .*-i $1 " &&
			pids="$pids ${p#/proc/}"
	done
	rm -f "$pf" "/var/run/nas.$1.pid"
	[ -n "$pids" ] || return 0
	kill $pids 2>/dev/null
	while [ $n -lt 10 ] && kill -0 $pids 2>/dev/null; do
		sleep 0.2; n=$((n + 1))
	done
	kill -9 $pids 2>/dev/null
	return 0
}

# canal + htmode do UCI -> chanspec do wl
chanspec() {
	local ch=$1 ht=$2 band=$3
	case "$ht" in
	*80)	echo "$ch/80" ;;
	*40*)
		if [ "$band" = 2g ]; then
			[ "$ch" -le 7 ] && echo "${ch}l" || echo "${ch}u"
		else
			[ $(( (ch / 4) % 2 )) = 1 ] && echo "${ch}l" || echo "${ch}u"
		fi ;;
	*)	echo "$ch" ;;
	esac
}

# cada wifi-iface "ap" do radio vira um BSS: o 1o na interface primaria
# (wlN), os demais em wlN.1, wlN.2... criados pelo wl (bss -C n up, MBSS)
NVIF=0
setup_vif() {
	local ssid encryption key hidden
	json_select config
	json_get_vars ssid encryption key hidden
	json_select ..
	eval "VIF_$NVIF=\$1 SSID_$NVIF=\$ssid ENC_$NVIF=\$encryption"
	eval "KEY_$NVIF=\$key HID_$NVIF=\${hidden:-0} BR_$NVIF=\$network_bridge"
	NVIF=$((NVIF + 1))
}

# cripto de um BSS (wsec/wpa_auth/closed na interface dele)
bss_sec() {
	local W="wl -i $1" enc=$2 hidden=$3
	$W closed "$hidden"
	case "$enc" in
	psk2*)	$W wsec 4; $W wpa_auth 128 ;;
	none|"")	$W wsec 0; $W wpa_auth 0 ;;
	*)	return 1 ;;
	esac
}

# nas (autenticador WPA2 do DD-WRT) de um BSS
nas_start() {
	local ifn=$1 ssid=$2 key=$3 br=$4 conf=/var/run/wl-nvram-$1.conf
	cat > "$conf" <<-EOT
	lan_ifname=${br:-br-lan}
	${ifn}_ifname=$ifn
	${ifn}_mode=ap
	${ifn}_akm=psk2
	${ifn}_crypto=aes
	${ifn}_ssid=$ssid
	${ifn}_wpa_gtk_rekey=3600
	EOT
	LD_LIBRARY_PATH=/usr/lib/ddwrt WL_NVRAM=$conf /usr/sbin/nas \
		-P "/var/run/nas.$ifn.pid" -H 34954 -l "${br:-$ifn}" \
		-i "$ifn" -A -m 128 -k "$key" -s "$ssid" -w 4 -g 3600 &
	echo $! > "/var/run/wl-nas.$ifn.pid"
	wireless_add_process $! /usr/sbin/nas 1
}

# BSS extras que o setup anterior deixou (lista em /var/run/wl-bss.<if>)
bss_down() {
	local ifname=$1 n
	for n in $(cat "/var/run/wl-bss.$ifname" 2>/dev/null); do
		nas_stop "$ifname.$n"
		wl -i "$ifname" bss -C "$n" down 2>/dev/null
	done
	rm -f "/var/run/wl-bss.$ifname"
}

drv_broadcom_setup() {
	local ifname country txpower channel htmode band W i n vif ifn
	local ssid enc key hidden br mac t

	json_select config
	json_get_vars ifname country txpower channel htmode band
	json_select ..
	W="wl -i $ifname"
	[ -d "/sys/class/net/$ifname" ] || { wireless_setup_failed NO_IFACE; return; }

	for_each_interface "ap" setup_vif
	[ "$NVIF" -gt 0 ] || { wireless_setup_failed NO_INTERFACES; return; }
	[ "$NVIF" -le 8 ] || { wireless_setup_failed TOO_MANY_INTERFACES; return; }
	[ -z "$channel" ] || [ "$channel" = auto ] && channel=$([ "$band" = 5g ] && echo 36 || echo 6)

	bss_down "$ifname"
	nas_stop "$ifname"
	$W down
	$W ap 1
	$W infra 1
	$W mbss $([ "$NVIF" -gt 1 ] && echo 1 || echo 0)
	# packet chaining na recepcao: wlc_rxframe_chainable le um ponteiro
	# NULL (+0x54) no primeiro trafego de cliente WPA e da panic em irq
	$W pktc 0
	[ -n "$country" ] && wlcountry "$ifname" "$country" > /dev/null
	$W chanspec "$(chanspec "$channel" "$htmode" "$band")"
	[ -n "$txpower" ] && $W txpwr1 -o -d "$txpower"

	i=0
	while [ $i -lt "$NVIF" ]; do
		eval "vif=\$VIF_$i ssid=\$SSID_$i enc=\$ENC_$i key=\$KEY_$i hidden=\$HID_$i br=\$BR_$i"
		case "$enc" in
		psk2*)	[ ${#key} -ge 8 ] || { wireless_setup_vif_failed INVALID_WPA_PSK; i=$((i + 1)); continue; } ;;
		none|"")	;;
		*)	wireless_setup_vif_failed UNSUPPORTED_ENCRYPTION; i=$((i + 1)); continue ;;
		esac
		if [ $i = 0 ]; then
			ifn=$ifname
			bss_sec "$ifn" "$enc" "$hidden"
			$W up
			$W ssid "$ssid"
		else
			n=$i; ifn=$ifname.$n
			$W ssid -C "$n" "$ssid"
			$W bss -C "$n" up
			# a netdev do BSS e registrada num work do driver
			t=0
			while [ ! -d "/sys/class/net/$ifn" ] && [ $t -lt 20 ]; do
				sleep 0.1; t=$((t + 1))
			done
			[ -d "/sys/class/net/$ifn" ] || { wireless_setup_vif_failed NO_BSS_IFACE; i=$((i + 1)); continue; }
			echo "$n" >> "/var/run/wl-bss.$ifname"
			# MAC que o wl gerou para o BSS (MBSS) tambem na netdev
			mac=$(wl -i "$ifn" cur_etheraddr | awk '{print $2}')
			[ -n "$mac" ] && ip link set "$ifn" address "$mac"
			bss_sec "$ifn" "$enc" "$hidden"
		fi
		ip link set "$ifn" up
		wireless_add_vif "$vif" "$ifn"
		case "$enc" in psk2*) nas_start "$ifn" "$ssid" "$key" "$br" ;; esac
		i=$((i + 1))
	done
	wireless_set_up
}

drv_broadcom_teardown() {
	local ifname
	json_select config
	json_get_vars ifname
	json_select ..
	bss_down "$ifname"
	nas_stop "$ifname"
	[ -d "/sys/class/net/$ifname" ] || return 0
	wl -i "$ifname" down
	ip link set "$ifname" down
}

add_driver broadcom
