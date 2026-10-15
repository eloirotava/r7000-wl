#!/bin/sh
# Roda um comando na box da LAN do R7000 (192.168.1.123) como root, com a
# senha de /root/password (informada pelo usuario), via jump no R7000.
# uso: box123.sh 'comando'
SSHPASS="$(cat /root/password)" exec sshpass -e ssh -o PubkeyAuthentication=no \
	-o StrictHostKeyChecking=accept-new -J root@10.8.0.11 root@192.168.1.123 "$1"
