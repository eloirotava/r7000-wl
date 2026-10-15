#!/bin/sh
# aplica a SHM do wl no b43 ja no ar e mede o macstat antes/depois
D=/sys/kernel/debug/b43/phy0
ms() {
	for o in 0xe0 0xea 0x10a 0x10c 0x10e 0x110; do
		echo "0x1 $o" > $D/shm16read
		printf "%s=%s " $o $(cat $D/shm16read)
	done
	echo
}
exec > /root/shmall.log 2>&1
echo antes; ms; sleep 3; ms
while read l; do echo "$l" > $D/shm16write; done < /tmp/shmall.txt
sync
echo depois; ms; sleep 3; ms; sleep 3; ms
dmesg | grep -E 'b43 rx' | head -20
echo FIM
