/*
 * wl_pkt.h - pacote "imitado" para o nucleo binario do wl, sem patch no
 * kernel.
 *
 * O nucleo binario (wlc_*, 7.14.164.18) acessa o pacote por offsets fixos
 * do sk_buff do kernel 4.4 do DD-WRT.  Em vez de reordenar o sk_buff do
 * kernel (patch 990), o nucleo e o codigo Broadcom compilado aqui
 * (shared/, wl_linux.c) trabalham com uma struct wl_pkt que tem esses
 * campos nesses offsets e aponta para o mesmo buffer de um sk_buff real
 * (sem copiar dados).  A conversao para o sk_buff real acontece so nas
 * bordas (wl_pkt.c): TX vindo da pilha, RX subindo a pilha e liberacao.
 *
 * Neste header:
 *  - sempre: a struct wl_pkt e os prototipos de wl_pkt.c;
 *  - fora de wl_pkt.c (WL_PKT_NATIVE nao definido): "struct sk_buff" vira
 *    "struct wl_pkt" e as funcoes de pacote do kernel usadas pelo codigo
 *    Broadcom viram as wlp_* que operam na imitacao.  Por isso todos os
 *    headers de kernel que esse codigo usa sao incluidos ANTES do #define.
 */
#ifndef _WL_PKT_H_
#define _WL_PKT_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/ieee80211.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <asm/cacheflush.h>
#include <asm/cache.h>
#include <asm/memory.h>
#include <net/sock.h>

/* o sk_buff do kernel, para o codigo de fronteira */
typedef struct sk_buff wl_native_skb;

/*
 * Layout que o nucleo binario espera (ARM32).  Os campos marcados [n]
 * sao lidos/escritos pelo nucleo por offset; os demais so pelo codigo
 * Broadcom compilado aqui, que usa os nomes.
 */
struct wl_pkt {
	struct wl_pkt		*next;		/* 0x00 [n] */
	struct wl_pkt		*prev;		/* 0x04 [n] */
	struct net_device	*dev;		/* 0x08 */
	void			*sk;		/* 0x0c */
	unsigned char		pktc_cb[8];	/* 0x10 [n] */
	unsigned char		ctf_pppoe_cb[8];/* 0x18 */
	void			*ctfmap;	/* 0x20 */
	void			*bcm_sk;	/* 0x24 */
	void			*bcm_dev;	/* 0x28 */
	__u32			bcm_pad0;	/* 0x2c */
	char			cb[48] __aligned(8); /* 0x30 [n] */
	void			(*destructor)(struct wl_pkt *); /* 0x60 */
	__u32			mark;		/* 0x64 */
	__u32			pad68;		/* 0x68 */
	void			*bcm_sp;	/* 0x6c */
	void			*ctfpool;	/* 0x70 [n] */
	__u32			pktc_flags;	/* 0x74 [n] */
	void			*ctf_ipc_txif;	/* 0x78 */
	__u32			napt_idx;	/* 0x7c */
	__u32			napt_flags;	/* 0x80 */
	__u32			truesize;	/* 0x84 */
	ktime_t			tstamp;		/* 0x88 */
	__u32			pad90;		/* 0x90 */
	unsigned int		len;		/* 0x94 [n] */
	unsigned int		data_len;	/* 0x98 [n] */
	__u16			mac_len;	/* 0x9c */
	__u16			hdr_len;	/* 0x9e */
	__wsum			csum;		/* 0xa0 */
	__be16			protocol;	/* 0xa4 */
	__u8			ip_summed;	/* 0xa6 */
	__u8			cloned;		/* 0xa7 */
	refcount_t		users;		/* 0xa8 */
	__u32			padac;		/* 0xac */
	__u32			padb0;		/* 0xb0 */
	__u32			priority;	/* 0xb4 [n] */
	/* 0xb8..0xdb: nao usados pelo nucleo; estado da imitacao */
	wl_native_skb		*native;	/* 0xb8: o sk_buff real */
	__u32			magic;		/* 0xbc */
	__u32			padc0[7];	/* 0xc0 */
	unsigned char		*tail;		/* 0xdc [n] */
	unsigned char		*end;		/* 0xe0 [n] */
	unsigned char		*head;		/* 0xe4 [n] */
	unsigned char		*data;		/* 0xe8 [n] */
	__u32			padec;		/* 0xec */
};

#define WL_PKT_MAGIC	0x77706b74	/* "wpkt" */

#define WL_PKT_AT(f, off) \
	static_assert(offsetof(struct wl_pkt, f) == (off), \
		"wl_pkt." #f " fora do offset do nucleo wl")
WL_PKT_AT(next, 0x00);
WL_PKT_AT(prev, 0x04);
WL_PKT_AT(pktc_cb, 0x10);
WL_PKT_AT(cb, 0x30);
WL_PKT_AT(ctfpool, 0x70);
WL_PKT_AT(pktc_flags, 0x74);
WL_PKT_AT(len, 0x94);
WL_PKT_AT(data_len, 0x98);
WL_PKT_AT(priority, 0xb4);
WL_PKT_AT(tail, 0xdc);
WL_PKT_AT(end, 0xe0);
WL_PKT_AT(head, 0xe4);
WL_PKT_AT(data, 0xe8);

/* fronteira (wl_pkt.c) */
struct wl_pkt *wlp_from_native(wl_native_skb *skb);
wl_native_skb *wlp_to_native(struct wl_pkt *p);
struct wl_pkt *wlp_dev_alloc(unsigned int len);
void wlp_free(struct wl_pkt *p);
struct wl_pkt *wlp_copy(struct wl_pkt *p);
struct wl_pkt *wlp_realloc_headroom(struct wl_pkt *p, unsigned int headroom);
int wlp_pad(struct wl_pkt *p, int pad);
int wlp_init(void);
void wlp_exit(void);

/* operacoes so na imitacao (tambem chamadas pelo nucleo: prep-core.sh) */
void *wlp_push(struct wl_pkt *p, unsigned int n);
void *wlp_pull(struct wl_pkt *p, unsigned int n);

static inline void wlp_trim(struct wl_pkt *p, unsigned int len)
{
	p->len = len;
	p->tail = p->data + len;
}

static inline void *wlp_put(struct wl_pkt *p, unsigned int n)
{
	unsigned char *t = p->tail;

	p->tail += n;
	p->len += n;
	return t;
}

static inline void wlp_reserve(struct wl_pkt *p, int n)
{
	p->data += n;
	p->tail += n;
}

static inline int wlp_tailroom(const struct wl_pkt *p)
{
	return p->data_len ? 0 : p->end - p->tail;
}

#ifndef WL_PKT_NATIVE
/*
 * Codigo Broadcom (shared/, wl_linux.c): o pacote e a imitacao.  Os
 * headers de kernel acima ja foram processados com o sk_buff real.
 */
#define sk_buff				wl_pkt
#undef skb_push
#undef skb_pull
#undef __skb_trim
#undef skb_trim
#undef skb_put
#undef skb_reserve
#undef skb_tailroom
#undef skb_headroom
#undef skb_pad
#undef skb_realloc_headroom
#undef dev_alloc_skb
#undef __dev_alloc_skb
#undef dev_kfree_skb
#undef dev_kfree_skb_any
#undef kfree_skb
#undef __kfree_skb
#undef skb_clone
#undef pskb_copy
#undef skb_copy
#undef skb_orphan
#undef skb_is_nonlinear
#undef skb_shinfo
#define skb_shinfo(s)			((struct skb_shared_info *)NULL)
#define skb_push(s, n)			wlp_push((s), (n))
#define skb_pull(s, n)			wlp_pull((s), (n))
#define __skb_trim(s, l)		wlp_trim((s), (l))
#define skb_trim(s, l)			wlp_trim((s), (l))
#define skb_put(s, n)			wlp_put((s), (n))
#define skb_reserve(s, n)		wlp_reserve((s), (n))
#define skb_tailroom(s)			wlp_tailroom(s)
#define skb_headroom(s)			((unsigned int)((s)->data - (s)->head))
#define skb_pad(s, n)			wlp_pad((s), (n))
#define skb_realloc_headroom(s, n)	wlp_realloc_headroom((s), (n))
#define dev_alloc_skb(l)		wlp_dev_alloc(l)
#define __dev_alloc_skb(l, g)		wlp_dev_alloc(l)
#define dev_kfree_skb(s)		wlp_free(s)
#define dev_kfree_skb_any(s)		wlp_free(s)
#define kfree_skb(s)			wlp_free(s)
#define __kfree_skb(s)			wlp_free(s)
#define skb_clone(s, g)			wlp_copy(s)
#define pskb_copy(s, g)			wlp_copy(s)
#define skb_copy(s, g)			wlp_copy(s)
#define skb_orphan(s)			do { (s)->destructor = NULL; } while (0)
#define skb_is_nonlinear(s)		((s)->data_len != 0)
#endif /* !WL_PKT_NATIVE */

#endif /* _WL_PKT_H_ */
