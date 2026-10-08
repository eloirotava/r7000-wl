// SPDX-License-Identifier: GPL-2.0
/*
 * osl_nvram.c - nvram_get() para o shared/ da Broadcom em cima do
 * bcm47xx_nvram do OpenWrt (particao "nvram" do CFE).
 *
 * As variaveis de calibracao dos radios ficam em "pci/<dominio>/<slot>/".
 * No kernel do DD-WRT os cores do SoC aparecem como dominio 0 e os
 * PCIe como 1 e 2; aqui os PCIe sao 0 e 1.  pcidom_offset corrige isso.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/bcm47xx_nvram.h>

static int pcidom_offset = 1;
module_param(pcidom_offset, int, 0444);
MODULE_PARM_DESC(pcidom_offset, "soma ao dominio PCI em chaves pci/N/... (padrao 1)");

#define NVRAM_VAL_MAX	256

struct nv_entry {
	struct nv_entry	*next;
	char		*name;
	char		*value;
};

static struct nv_entry *nv_cache;
static DEFINE_SPINLOCK(nv_lock);

static const char *nv_remap(const char *name, char *buf, size_t len)
{
	unsigned int dom;
	int n;

	if (!pcidom_offset || strncmp(name, "pci/", 4))
		return name;
	if (sscanf(name + 4, "%u%n", &dom, &n) != 1 || name[4 + n] != '/')
		return name;
	snprintf(buf, len, "pci/%u%s", dom + pcidom_offset, name + 4 + n);
	return buf;
}

/* o shared/ espera ponteiros validos ate o fim: guardamos em cache */
char *nvram_get(const char *name)
{
	char key[96], *val;
	const char *real;
	struct nv_entry *e;
	unsigned long flags;

	if (!name)
		return NULL;

	spin_lock_irqsave(&nv_lock, flags);
	for (e = nv_cache; e; e = e->next) {
		if (!strcmp(e->name, name)) {
			spin_unlock_irqrestore(&nv_lock, flags);
			return e->value;
		}
	}
	spin_unlock_irqrestore(&nv_lock, flags);

	val = kmalloc(NVRAM_VAL_MAX, GFP_ATOMIC);
	if (!val)
		return NULL;

	real = nv_remap(name, key, sizeof(key));
	if (bcm47xx_nvram_getenv(real, val, NVRAM_VAL_MAX) < 0) {
		kfree(val);
		return NULL;
	}

	e = kmalloc(sizeof(*e), GFP_ATOMIC);
	if (!e || !(e->name = kstrdup(name, GFP_ATOMIC))) {
		kfree(e);
		kfree(val);
		return NULL;
	}
	e->value = val;

	spin_lock_irqsave(&nv_lock, flags);
	e->next = nv_cache;
	nv_cache = e;
	spin_unlock_irqrestore(&nv_lock, flags);
	return val;
}

void wl_nvram_free(void)
{
	struct nv_entry *e;

	while ((e = nv_cache)) {
		nv_cache = e->next;
		kfree(e->name);
		kfree(e->value);
		kfree(e);
	}
}
