/*
 * wl_abi.h - contrato entre a camada de SO (wl_linux.c) e o nucleo
 * binario do wl Broadcom 7.14.164.18 (r692288) usado no DD-WRT.
 *
 * Os headers internos (wlc_pub.h, wl_export.h) dessa versao nao sao
 * publicos.  Prototipos conferidos contra o desassembly do wl_linux.o
 * do DD-WRT; offsets do wlc_pub_t medidos no mesmo desassembly.
 */
#ifndef _WL_ABI_H_
#define _WL_ABI_H_

#include <typedefs.h>
#include <osl.h>

struct wl_info;
struct wl_if;
struct wlc_info;
struct wlc_if;
struct wl_timer;
struct bcmstrbuf;

typedef struct wlc_pub wlc_pub_t;	/* opaco: acesso so pelos macros abaixo */
typedef struct wlc_event wlc_event_t;	/* opaco */
typedef struct wl_rxsts wl_rxsts_t;	/* opaco */

/* wlc_pub_t (ARM32) */
#define WLPUB_CUR_ETHERADDR(pub)	((uint8 *)(pub) + 0x04)
#define WLPUB_UNIT(pub)			(*(uint *)((uint8 *)(pub) + 0x0c))
#define WLPUB_UP(pub)			(*(volatile bool *)((uint8 *)(pub) + 0x18))
#define WLPUB_HW_UP(pub)		(*(volatile bool *)((uint8 *)(pub) + 0x20))

/* bustype (bcmdevs.h) */
#ifndef PCI_BUS
#define PCI_BUS		1
#endif

/* ioctls WLC_* usados aqui (wlioctl.h) */
#define WLC_SET_PROMISC_ABI	10

/* wl_get_ifctx() */
#define IFCTX_NETDEV		3

/* ---- camada de SO -> nucleo ---- */
extern void *wlc_attach(void *wl, uint16 vendor, uint16 device, uint unit,
	bool piomode, osl_t *osh, void *regsva, uint bustype, void *btparam,
	void *objr, uint *perr);
extern uint wlc_detach(struct wlc_info *wlc);
extern wlc_pub_t *wlc_pub(void *wlc);
extern bool wlc_chipmatch(uint16 vendor, uint16 device);
extern int wlc_up(struct wlc_info *wlc);
extern uint wlc_down(struct wlc_info *wlc);
extern void wlc_init(struct wlc_info *wlc);
extern void wlc_reset(struct wlc_info *wlc);
extern bool wlc_isr(struct wlc_info *wlc, bool *wantdpc);
extern bool wlc_dpc(struct wlc_info *wlc, bool bounded, void *dpc);
extern void wlc_intrson(struct wlc_info *wlc);
extern uint32 wlc_intrsoff(struct wlc_info *wlc);
extern void wlc_intrsrestore(struct wlc_info *wlc, uint32 macintmask);
extern bool wlc_intrsupd(struct wlc_info *wlc);
extern bool wlc_sendpkt(struct wlc_info *wlc, void *sdu, struct wlc_if *wlcif);
extern int wlc_ioctl(struct wlc_info *wlc, int cmd, void *arg, int len,
	struct wlc_if *wlcif);
extern int wlc_get(struct wlc_info *wlc, int cmd, int *arg);
extern int wlc_set(struct wlc_info *wlc, int cmd, int arg);
extern int wlc_iovar_op(struct wlc_info *wlc, const char *name, void *params,
	int p_len, void *arg, int len, bool set, struct wlc_if *wlcif);
extern int wlc_iovar_getint(struct wlc_info *wlc, const char *name, int *arg);
extern int wlc_iovar_setint(struct wlc_info *wlc, const char *name, int arg);
extern struct wlc_if *wlc_wlcif_get_by_index(struct wlc_info *wlc, uint idx);
typedef void (*wl_watchdog_fn_t)(void *handle);
extern int wlc_module_register(wlc_pub_t *pub, const void *iovars,
	const char *name, void *hdl, void *iovar_fn,
	wl_watchdog_fn_t watchdog_fn, void *up_fn, void *down_fn);
extern int wlc_module_unregister(wlc_pub_t *pub, const char *name, void *hdl);

/* ---- nucleo -> camada de SO (implementadas em wl_linux.c) ---- */
extern void wl_init(struct wl_info *wl);
extern uint wl_reset(struct wl_info *wl);
extern void wl_intrson(struct wl_info *wl);
extern uint32 wl_intrsoff(struct wl_info *wl);
extern void wl_intrsrestore(struct wl_info *wl, uint32 macintmask);
extern int wl_up(struct wl_info *wl);
extern void wl_down(struct wl_info *wl);
extern void wl_event(struct wl_info *wl, char *ifname, wlc_event_t *e);
extern void wl_event_sync(struct wl_info *wl, char *ifname, wlc_event_t *e);
extern void wl_dump_ver(struct wl_info *wl, struct bcmstrbuf *b);
extern void wl_txflowcontrol(struct wl_info *wl, struct wl_if *wlif, bool state,
	int prio);
extern bool wl_alloc_dma_resources(struct wl_info *wl, uint dmaddrwidth);
extern struct wl_timer *wl_init_timer(struct wl_info *wl, void (*fn)(void *arg),
	void *arg, const char *name);
extern void wl_free_timer(struct wl_info *wl, struct wl_timer *timer);
extern void wl_add_timer(struct wl_info *wl, struct wl_timer *timer, uint ms,
	int periodic);
extern bool wl_del_timer(struct wl_info *wl, struct wl_timer *timer);
extern void wl_sendup(struct wl_info *wl, struct wl_if *wlif, void *p, int numpkt);
extern char *wl_ifname(struct wl_info *wl, struct wl_if *wlif);
extern struct wl_if *wl_add_if(struct wl_info *wl, struct wlc_if *wlcif,
	uint unit, struct ether_addr *remote);
extern void wl_del_if(struct wl_info *wl, struct wl_if *wlif);
extern void *wl_get_ifctx(struct wl_info *wl, int ctx_id, struct wl_if *wlif);
extern void wl_monitor(struct wl_info *wl, wl_rxsts_t *rxsts, void *p);
extern void wl_set_monitor(struct wl_info *wl, int val);
extern void *wl_get_wireless_stats(void *dev);
extern int wl_osl_pcie_rc(struct wl_info *wl, uint op, int param);

#endif /* _WL_ABI_H_ */
