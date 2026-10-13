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
nas_stop() {
	local pf=/var/run/wl-nas.$1.pid
	[ -s "$pf" ] && kill "$(cat "$pf")" 2>/dev/null
	rm -f "$pf"
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

VIF=; BRIDGE=
setup_vif() {
	[ -z "$VIF" ] || { wireless_setup_vif_failed ONLY_ONE_AP_PER_RADIO; return; }
	VIF=$1
	BRIDGE=$network_bridge
	json_select config
	json_get_vars ssid encryption key hidden
	json_select ..
}

drv_broadcom_setup() {
	local ifname country txpower channel htmode band W conf
	local ssid encryption key hidden

	json_select config
	json_get_vars ifname country txpower channel htmode band
	json_select ..
	W="wl -i $ifname"
	[ -d "/sys/class/net/$ifname" ] || { wireless_setup_failed NO_IFACE; return; }

	for_each_interface "ap" setup_vif
	[ -n "$VIF" ] || { wireless_setup_failed NO_INTERFACES; return; }
	[ -z "$channel" ] || [ "$channel" = auto ] && channel=$([ "$band" = 5g ] && echo 36 || echo 6)

	nas_stop "$ifname"
	$W down
	$W ap 1
	$W infra 1
	# packet chaining na recepcao: wlc_rxframe_chainable le um ponteiro
	# NULL (+0x54) no primeiro trafego de cliente WPA e da panic em irq
	$W pktc 0
	[ -n "$country" ] && wlcountry "$ifname" "$country" > /dev/null
	$W chanspec "$(chanspec "$channel" "$htmode" "$band")"
	[ -n "$txpower" ] && $W txpwr1 -o -d "$txpower"
	$W closed "${hidden:-0}"
	case "$encryption" in
	psk2*)	$W wsec 4; $W wpa_auth 128 ;;
	none|"")	$W wsec 0; $W wpa_auth 0 ;;
	*)	wireless_setup_vif_failed UNSUPPORTED_ENCRYPTION; return ;;
	esac
	$W up
	$W ssid "$ssid"
	ip link set "$ifname" up
	wireless_add_vif "$VIF" "$ifname"

	if [ "$encryption" != none ] && [ -n "$encryption" ]; then
		[ ${#key} -ge 8 ] || { wireless_setup_vif_failed INVALID_WPA_PSK; return; }
		conf=/var/run/wl-nvram-$ifname.conf
		cat > "$conf" <<-EOT
		lan_ifname=${BRIDGE:-br-lan}
		${ifname}_ifname=$ifname
		${ifname}_mode=ap
		${ifname}_akm=psk2
		${ifname}_crypto=aes
		${ifname}_ssid=$ssid
		${ifname}_wpa_gtk_rekey=3600
		EOT
		LD_LIBRARY_PATH=/usr/lib/ddwrt WL_NVRAM=$conf /usr/sbin/nas \
			-P "/var/run/nas.$ifname.pid" -H 34954 -l "${BRIDGE:-$ifname}" \
			-i "$ifname" -A -m 128 -k "$key" -s "$ssid" -w 4 -g 3600 &
		echo $! > "/var/run/wl-nas.$ifname.pid"
		wireless_add_process $! /usr/sbin/nas 1
	fi
	wireless_set_up
}

drv_broadcom_teardown() {
	local ifname
	json_select config
	json_get_vars ifname
	json_select ..
	nas_stop "$ifname"
	[ -d "/sys/class/net/$ifname" ] || return 0
	wl -i "$ifname" down
	ip link set "$ifname" down
}

add_driver broadcom
