/*
 * wl_shim.h - simbolos que shim.c e osl_nvram.c exportam para o nucleo
 * binario e para o shared/ da Broadcom.
 */
#ifndef _WL_SHIM_H_
#define _WL_SHIM_H_

#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/dma-direction.h>

struct device;
struct net_device;

/* shim.c */
extern void *wl_kmalloc_caches[];
void *wl_kmem_cache_alloc(void *cache, gfp_t flags);
void *wl___kmalloc(size_t size, gfp_t flags);
void __memzero(void *p, size_t n);
void wireless_send_event(struct net_device *dev, unsigned int cmd,
	void *wrqu, const char *extra);
int emfc_init(void *a, void *b, void *c, void *d, void *e, void *f, void *g);
void emfc_exit(void *emfc);
int emfc_input(void *emfc, void *skb, void *ifp, unsigned char *iph,
	int rt_port);
void emfc_cfg_request_process(void *cfg);
void *igsc_init(void *a, void *b, void *c, void *d, void *e);
void igsc_exit(void *igsc);
int igsc_interface_rtport_del(void *igsc, void *ifp);
int igsc_sdb_interface_del(void *igsc, void *ifp);
extern struct device *wl_dma_dev;

/* osl_nvram.c */
char *nvram_get(const char *name);
int nvram_init(void *sih);
void nvram_exit(void *sih);
int nvram_match(const char *name, const char *match);
int nvram_getall(char *buf, int count);
void wl_nvram_free(void);

#endif /* _WL_SHIM_H_ */
