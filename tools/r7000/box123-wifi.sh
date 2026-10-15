#!/bin/sh
# Poe a box 192.168.1.123 no wifi do R7000 (Lab513-IoT) via netplan; a senha
# vai do uci do R7000 direto para a box (nao aparece na saida).  O cabo
# (end0) continua, entao o acesso nao depende do wifi.
R=root@10.8.0.11
B="-J $R root@192.168.1.123"
K=$(timeout 15 ssh $R 'uci -q get wireless.default_radio0.key')
[ -n "$K" ] || { echo "sem chave no R7000"; exit 1; }
printf '%s' "$K" | timeout 60 ssh $B '
set -e
k=$(cat)
cp /etc/netplan/30-wifi.yaml /etc/netplan/30-wifi.yaml.bak-$(date +%Y%m%d-%H%M%S)
umask 077
cat > /etc/netplan/30-wifi.yaml <<EOF
network:
  version: 2
  renderer: networkd
  wifis:
    wlan0:
      dhcp4: true
      dhcp4-overrides:
        route-metric: 600
      access-points:
        "Lab513-IoT":
          password: "$k"
EOF
chmod 600 /etc/netplan/30-wifi.yaml
netplan apply
sleep 15
iw dev wlan0 link | head -4
ip -4 -br addr show wlan0
ip route | head -3
'
