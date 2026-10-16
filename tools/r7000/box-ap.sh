#!/bin/sh
# Roda NA box BCM4335 (10.8.0.20): AP aberto de teste para o b43.
# uso: box-ap.sh start [canal] [wpa] | stop
# start: para o cliente wifi do netplan e sobe o hostapd (log -dd em
# /root/b43ap.log); stop: derruba o hostapd e devolve o wlan0 ao netplan.
set -e
case "$1" in
start)
	CH=${2:-44}
	if [ "$CH" -le 14 ]; then HW=g; else HW=a; fi
	systemctl stop netplan-wpa-wlan0 2>/dev/null || true
	pkill -x hostapd 2>/dev/null || true
	ip link set wlan0 down
	cat > /root/b43ap.conf <<EOF
interface=wlan0
driver=nl80211
ssid=b43-teste
country_code=BR
hw_mode=$HW
channel=$CH
ieee80211n=1
auth_algs=1
EOF
	# box-ap.sh start <canal> wpa: WPA2-PSK/CCMP (senha b43teste123)
	if [ "$3" = wpa ]; then
		printf 'wpa=2\nwpa_key_mgmt=WPA-PSK\nrsn_pairwise=CCMP\nwpa_passphrase=b43teste123\n' >> /root/b43ap.conf
	fi
	hostapd -B -dd -f /root/b43ap.log /root/b43ap.conf
	sleep 3
	iw dev wlan0 info | grep -E 'type|channel'
	;;
stop)
	pkill -x hostapd 2>/dev/null || true
	sleep 1
	ip link set wlan0 down
	systemctl start netplan-wpa-wlan0 2>/dev/null || netplan apply
	sleep 10
	iw dev wlan0 link | head -3
	;;
esac
