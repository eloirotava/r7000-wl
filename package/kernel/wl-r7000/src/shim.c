// SPDX-License-Identifier: GPL-2.0
/*
 * shim.c - simbolos que o nucleo binario do wl (compilado contra o
 * kernel 4.4 do DD-WRT) importa e que nao existem, ou mudaram, no 6.12.
 *
 * O Kbuild renomeia nos objetos do nucleo (objcopy --redefine-sym):
 *   kmalloc_caches   -> wl_kmalloc_caches
 *   kmem_cache_alloc -> wl_kmem_cache_alloc
 *   __kmalloc        -> wl___kmalloc
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

/*
 * kmalloc() inline do 4.4 com tamanho constante vira
 * kmem_cache_alloc(kmalloc_caches[idx], flags).  Cada "cache" aqui e so
 * o indice codificado no ponteiro; o tamanho sai do indice como no 4.4
 * (1 = 96 bytes, 2 = 192 bytes, n = 1 << n).
 */
#define WL_KMALLOC_SHIFT_HIGH	(PAGE_SHIFT + 1)

#define C(n)	((void *)(n))
void *wl_kmalloc_caches[WL_KMALLOC_SHIFT_HIGH + 1] = {
	NULL, C(1), C(2), C(3), C(4), C(5), C(6), C(7),
	C(8), C(9), C(10), C(11), C(12), C(13),
};
#undef C
static_assert(WL_KMALLOC_SHIFT_HIGH == 13, "ajustar wl_kmalloc_caches");

static size_t wl_kmalloc_idx_size(unsigned long idx)
{
	if (idx == 1)
		return 96;
	if (idx == 2)
		return 192;
	return (size_t)1 << idx;
}

void *wl_kmem_cache_alloc(void *cache, gfp_t flags)
{
	unsigned long idx = (unsigned long)cache;

	if (idx == 0 || idx > WL_KMALLOC_SHIFT_HIGH)
		return NULL;
	return kmalloc(wl_kmalloc_idx_size(idx), flags);
}

void *wl___kmalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags);
}

/* sem janela ACP no mainline (ver osl_compat.h) */
unsigned int ns_acp_win_size;

/* lock da WAR de leitura PCIe x L2 (so ativa se PHYS_OFFSET == 0x80000000) */
DEFINE_SPINLOCK(l2x0_reg_lock);

/* WAR de PCIe gen2 do 4360 que vive no arch do kernel Broadcom */
void do_4360_pcie2_war(void)
{
	/* TODO: portar de arch/arm/mach-bcm5301x (PCIe gen2 + 4360) */
}

/* WEXT: sem uso no OpenWrt */
void wireless_send_event(struct net_device *dev, unsigned int cmd,
	void *wrqu, const char *extra)
{
}

/* EMF / IGS (snooping multicast da Broadcom): desligados */
int emfc_init(void *a, void *b, void *c, void *d, void *e, void *f, void *g)
{
	return 0;
}

void emfc_exit(void *emfc)
{
}

int emfc_input(void *emfc, void *skb, void *ifp, unsigned char *iph, int rt_port)
{
	return 0;	/* EMF_NOP: segue o caminho normal */
}

void emfc_cfg_request_process(void *cfg)
{
}

void *igsc_init(void *a, void *b, void *c, void *d, void *e)
{
	return NULL;
}

void igsc_exit(void *igsc)
{
}

int igsc_interface_rtport_del(void *igsc, void *ifp)
{
	return 0;
}

int igsc_sdb_interface_del(void *igsc, void *ifp)
{
	return 0;
}
