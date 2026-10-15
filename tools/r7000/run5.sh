#!/bin/sh
# Na VPS: envia o b43 (artefato em /root/r7000-b43/b43m4) e roda o teste
# do radio de 5 GHz (0001:01:00.0, ac_band=5) com o testguard armado.
R=root@10.8.0.11
B=/root/r7000-b43
T=$(dirname "$0")
timeout 30 ssh $R 'mkdir -p /tmp/b43 /tmp/fw/b43; echo ac_band=5 tx_hexdump=3 > /tmp/b43args; rm -f /root/b43.log /root/b43dm.txt' || exit 1
scp -O -q $B/b43m4/out/*.ko $R:/tmp/b43/ || exit 1
scp -O -q $B/fw/*.fw $R:/tmp/fw/b43/ || exit 1
scp -O -q $T/b43test5.sh $R:/tmp/b43test.sh || exit 1
timeout 30 ssh $R 'echo 0 > /etc/wl-r7000.fails; testguard 10; start-stop-daemon -S -b -n b43test -x /bin/sh -- /tmp/b43test.sh; echo disparado'
