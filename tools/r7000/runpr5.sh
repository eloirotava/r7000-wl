#!/bin/sh
# Na VPS: b43 da serie do PR openwrt 25788 (artefato em $1) + firmware
# 6.30.163.46 (/root/r7000-b43/fw630/b43) no radio de 5 GHz, testguard 10 min.
# Antes: tools/r7000/box-ap.sh start 44 na box .20 (AP aberto b43-teste).
R=root@10.8.0.11
# B43ARGS: parametros extras do b43 (ex.: acphy_rpcal5g da NVRAM pci/2/1/)
A=${1:?diretorio com os .ko}
T=$(dirname "$0")
timeout 30 ssh $R "rm -rf /tmp/b43 /tmp/fw; mkdir -p /tmp/b43 /tmp/fw/b43; echo '$B43ARGS' > /tmp/b43args; rm -f /root/b43.log /root/b43dm.txt" || exit 1
scp -O -q $A/*.ko $R:/tmp/b43/ || exit 1
scp -O -q /root/r7000-b43/fw630/b43/ucode42.fw /root/r7000-b43/fw630/b43/ac1*initvals42.fw $R:/tmp/fw/b43/ || exit 1
scp -O -q $T/b43pr5.sh $R:/tmp/b43test.sh || exit 1
timeout 30 ssh $R 'echo 0 > /etc/wl-r7000.fails; testguard 10; start-stop-daemon -S -b -n b43test -x /bin/sh -- /tmp/b43test.sh; echo disparado'
