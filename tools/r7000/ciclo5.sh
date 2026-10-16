#!/bin/sh
# Na VPS: espera o build do HEAD (b43.yml), o R7000 voltar com o wl, roda o
# teste do 5 GHz (controle) (runpr5.sh) e conta pacotes em monitor nos canais 6 e 1.
cd "$(dirname "$0")/../.."
H=$(git log -1 --format=%H -- b43 .github/workflows/b43.yml)
until R=$(gh run list -b b43-pr25788 -L5 --json databaseId,headSha --jq ".[]|select(.headSha==\"$H\")|.databaseId" | head -1); [ -n "$R" ]; do sleep 5; done
echo "run=$R"
gh run watch "$R" --exit-status >/dev/null 2>&1 || { echo BUILD_FALHOU; exit 1; }
D=/root/.claude/jobs/83aef213/tmp/r7k-ciclo; rm -rf $D; mkdir -p $D; gh run download "$R" -D $D
until timeout 10 ssh -o ConnectTimeout=6 root@10.8.0.11 'awk "{exit !(\$1<600)}" /proc/uptime && lsmod | grep -q "^wl "' 2>/dev/null; do sleep 2; done
B43ARGS="${B43ARGS:-acphy_rpcal5g=0x5af5,0x6ded,0x60ef,0x7a01 acphy_maxp5g=106,106,106,106,106,106,106,106,106,106,106,106}" tools/r7000/runpr5.sh $D/b43-ac/out || exit 1
until timeout 15 ssh -o ConnectTimeout=8 root@10.8.0.11 'grep -q FIM /root/b43.log' 2>/dev/null; do sleep 2; done
timeout 120 ssh root@10.8.0.11 'export LD_LIBRARY_PATH=/tmp/iwx/r/usr/lib; I=/tmp/iwx/r/usr/sbin/iw; sed -n "/scan dump/,/connect/p" /root/b43.log | grep -E "^BSS|signal|SSID" | head -10; grep -E "wlb0: (auth|assoc)|Connected|Not connected|packet loss" /root/b43.log | tail -5; ip link set wlb0 down; $I dev wlb0 set type monitor; ip link set wlb0 up; for c in 44 36; do $I dev wlb0 set channel $c; a=$(cat /sys/class/net/wlb0/statistics/rx_packets); sleep 5; b=$(cat /sys/class/net/wlb0/statistics/rx_packets); echo "canal $c: rx $((b-a)) em 5 s"; done'
