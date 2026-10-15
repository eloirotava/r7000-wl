#!/bin/sh
# Na VPS: grava com o wltrace as trocas de canal do wl0 (2,4 GHz) em cada
# sub-banda.  Usa o wl rastreado ja instalado em /etc/wltrace (gancho
# one-shot do init) e reinicia o R7000; o rastro fica em /root/t-chan24.bin.
R=root@10.8.0.11
timeout 60 ssh $R '
set -e
rm -f /etc/wltrace/mask /root/t-chan24.bin /root/chancap24.log
cat > /etc/wltrace/dump.sh <<"EOF"
#!/bin/sh
exec > /root/chancap24.log 2>&1
sleep 90
for c in 1 6 11; do
	echo "troca $c $(cat /proc/uptime)"
	wl -i wl0 down
	wl -i wl0 chanspec $c
	wl -i wl0 up
	sleep 20
	wl -i wl0 chanspec
done
cat /proc/wltrace > /root/t-chan24.bin
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
