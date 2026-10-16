/*
 * bpdump: le registradores do backplane de uma placa bcma PCIe (BCM4360 do
 * R7000) pelo BAR0, com o driver (wl ou b43) no ar.
 *
 * BAR0 da placa: 0x0000..0x0fff = janela do core (cfg 0x80),
 *                0x1000..0x1fff = janela do wrapper (cfg 0x70).
 * As duas janelas sao salvas e restauradas no fim.
 *
 * uso: bpdump <dev pci> <core base> <wrapper base> <offset>...
 *   offsets "c:0x28" = core, "w:0x408" = wrapper,
 *   "cci:N" = ChipCommon PMU chipcontrol N (addr 0x650 / data 0x654),
 *   "cri:N" = regcontrol N (0x658/0x65c), "cpl:N" = pllcontrol N (0x660/0x664)
 *   (os indiretos so fazem sentido com core base = ChipCommon)
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int cfg;

static uint32_t cfg_rd(int off)
{
	uint32_t v = 0;

	pread(cfg, &v, 4, off);
	return v;
}

static void cfg_wr(int off, uint32_t v)
{
	pwrite(cfg, &v, 4, off);
}

int main(int argc, char **argv)
{
	char path[256];
	volatile uint32_t *bar;
	uint32_t core, wrap, w1, w2;
	int fd, i;

	if (argc < 5) {
		fprintf(stderr, "uso: %s 0000:01:00.0 0x18000000 0x18100000 c:0x0 ...\n", argv[0]);
		return 1;
	}
	core = strtoul(argv[2], NULL, 0);
	wrap = strtoul(argv[3], NULL, 0);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/config", argv[1]);
	cfg = open(path, O_RDWR);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/resource0", argv[1]);
	fd = open(path, O_RDWR | O_SYNC);
	if (cfg < 0 || fd < 0) {
		perror("open");
		return 1;
	}
	bar = mmap(NULL, 0x2000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (bar == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	w1 = cfg_rd(0x80);
	w2 = cfg_rd(0x70);
	cfg_wr(0x80, core);
	cfg_wr(0x70, wrap);
	for (i = 4; i < argc; i++) {
		char *a = argv[i];
		uint32_t off = strtoul(strchr(a, ':') + 1, NULL, 0), v;

		if (!strncmp(a, "c:", 2)) {
			v = bar[off / 4];
		} else if (!strncmp(a, "w:", 2)) {
			v = bar[(0x1000 + off) / 4];
		} else {
			uint32_t ao = !strncmp(a, "cci:", 4) ? 0x650 :
				      !strncmp(a, "cri:", 4) ? 0x658 : 0x660;

			bar[ao / 4] = off;
			(void)bar[ao / 4];
			v = bar[(ao + 4) / 4];
		}
		printf("%s 0x%08x\n", a, v);
	}
	cfg_wr(0x80, w1);
	cfg_wr(0x70, w2);
	return 0;
}
