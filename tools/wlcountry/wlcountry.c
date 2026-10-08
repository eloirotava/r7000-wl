// SPDX-License-Identifier: GPL-2.0
/*
 * wlcountry - le/define o pais + revisao (CLM) do driver wl da Broadcom.
 * O utilitario wl do DD-WRT trata "country" como inteiro e nao aceita
 * revisao.  Uso: wlcountry <if>           (mostra)
 *                wlcountry <if> CC[/rev]  (define; radio em "wl down")
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

typedef struct {
	char country_abbrev[4];
	int32_t rev;
	char ccode[4];
} wl_country_t;

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

int main(int argc, char **argv)
{
	char buf[64] = "country";
	wl_country_t c;
	char *slash;

	if (argc < 2) {
		fprintf(stderr, "uso: %s <if> [CC[/rev]]\n", argv[0]);
		return 2;
	}

	if (argc > 2) {
		memset(&c, 0, sizeof(c));
		c.rev = -1;
		slash = strchr(argv[2], '/');
		if (slash) {
			*slash = '\0';
			c.rev = atoi(slash + 1);
		}
		strncpy(c.country_abbrev, argv[2], 3);
		strncpy(c.ccode, argv[2], 3);
		memcpy(buf + sizeof("country"), &c, sizeof(c));
		if (wl_ioctl(argv[1], WLC_SET_VAR, buf, sizeof("country") + sizeof(c), 1)) {
			perror("set country");
			return 1;
		}
	}

	memset(buf, 0, sizeof(buf));
	strcpy(buf, "country");
	if (wl_ioctl(argv[1], WLC_GET_VAR, buf, sizeof(buf), 0)) {
		perror("get country");
		return 1;
	}
	memcpy(&c, buf, sizeof(c));
	printf("%.4s/%d (ccode %.4s)\n", c.country_abbrev, c.rev, c.ccode);
	return 0;
}
