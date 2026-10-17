// SPDX-License-Identifier: GPL-2.0
/*
 * wlbss - liga/desliga um BSS extra (MBSS) e define o MAC dele no driver wl
 * da Broadcom.  O utilitario wl do DD-WRT nao tem o comando "bss" (trata
 * como iovar inteiro e o driver responde "Buffer too short") nem MAC por
 * bsscfg.  Usa o prefixo "bsscfg:" (nome, '\0', indice int32, dado).
 * Uso: wlbss <if> <n>                (mostra 0/1)
 *      wlbss <if> <n> up|down
 *      wlbss <if> <n> mac XX:XX:XX:XX:XX:XX   (com o BSS em down)
 */
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <unistd.h>

#define WLC_GET_VAR	262
#define WLC_SET_VAR	263

typedef struct {
	unsigned int cmd;
	void *buf;
	unsigned int len;
	uint8_t set;
	unsigned int used;
	unsigned int needed;
} wl_ioctl_t;

static int wl_ioctl(const char *ifname, int cmd, void *buf, int len, int set)
{
	struct ifreq ifr;
	wl_ioctl_t ioc = { .cmd = cmd, .buf = buf, .len = len, .set = set };
	int s = socket(AF_INET, SOCK_DGRAM, 0), r;

	if (s < 0)
		return -1;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	ifr.ifr_data = (void *)&ioc;
	r = ioctl(s, SIOCDEVPRIVATE, &ifr);
	close(s);
	return r;
}

/* "bsscfg:<nome>\0" + int32 indice + dado; devolve o tamanho */
static int bsscfg_buf(char *buf, const char *name, int32_t idx,
	const void *data, int dlen)
{
	int n = sprintf(buf, "bsscfg:%s", name) + 1;

	memcpy(buf + n, &idx, sizeof(idx));
	n += sizeof(idx);
	if (data) {
		memcpy(buf + n, data, dlen);
		n += dlen;
	}
	return n;
}

int main(int argc, char **argv)
{
	char buf[128];
	int32_t idx, val;
	unsigned int m[6];
	uint8_t mac[6];
	int i, len;

	if (argc < 3) {
		fprintf(stderr, "uso: %s <if> <n> [up|down|mac XX:XX:XX:XX:XX:XX]\n", argv[0]);
		return 2;
	}
	idx = atoi(argv[2]);

	if (argc == 3) {
		memset(buf, 0, sizeof(buf));
		bsscfg_buf(buf, "bss", idx, NULL, 0);
		if (wl_ioctl(argv[1], WLC_GET_VAR, buf, sizeof(buf), 0)) {
			perror("get bss");
			return 1;
		}
		memcpy(&val, buf, sizeof(val));
		printf("%d\n", val);
		return 0;
	}

	if (!strcmp(argv[3], "up") || !strcmp(argv[3], "down")) {
		val = !strcmp(argv[3], "up");
		len = bsscfg_buf(buf, "bss", idx, &val, sizeof(val));
		if (wl_ioctl(argv[1], WLC_SET_VAR, buf, len, 1)) {
			perror("set bss");
			return 1;
		}
		return 0;
	}

	if (!strcmp(argv[3], "mac") && argc > 4) {
		if (sscanf(argv[4], "%x:%x:%x:%x:%x:%x",
			&m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
			fprintf(stderr, "MAC invalido: %s\n", argv[4]);
			return 2;
		}
		for (i = 0; i < 6; i++)
			mac[i] = m[i];
		len = bsscfg_buf(buf, "cur_etheraddr", idx, mac, sizeof(mac));
		if (wl_ioctl(argv[1], WLC_SET_VAR, buf, len, 1)) {
			perror("set cur_etheraddr");
			return 1;
		}
		return 0;
	}

	fprintf(stderr, "comando desconhecido: %s\n", argv[3]);
	return 2;
}
