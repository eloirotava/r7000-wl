#!/bin/sh
# Na VPS: grava com o wltrace as trocas de canal do wl1 (5 GHz) em cada
# sub-banda.  Usa o wl rastreado ja instalado em /etc/wltrace (gancho
# one-shot do init) e reinicia o R7000; o rastro fica em /root/t-chan.bin.
R=root@10.8.0.11
timeout 60 ssh $R '
set -e
rm -f /etc/wltrace/mask /root/t-chan.bin /root/chancap.log
cat > /etc/wltrace/dump.sh <<"EOF"
#!/bin/sh
exec > /root/chancap.log 2>&1
sleep 90
for c in 44 100 149 36; do
	echo "troca $c $(cat /proc/uptime)"
	wl -i wl1 down
	wl -i wl1 chanspec $c
	wl -i wl1 up
	sleep 20
	wl -i wl1 chanspec
done
cat /proc/wltrace > /root/t-chan.bin
sync
echo FIM
EOF
chmod +x /etc/wltrace/dump.sh
echo 0 > /etc/wl-r7000.fails
touch /etc/wltrace-once
sync
testguard 15
reboot
'
