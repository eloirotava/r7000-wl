/*
 * wl_pkt.c - fronteira entre a imitacao (struct wl_pkt, que o nucleo
 * binario e o codigo Broadcom usam) e o sk_buff real do kernel.
 * Compilado com WL_PKT_NATIVE: aqui "struct sk_buff" e o do kernel.
 *
 * A imitacao aponta para o buffer do sk_buff real: head/data/tail/end sao
 * ponteiros dentro dele e nada e copiado.  O nucleo so mexe nos ponteiros
 * e no len da imitacao; o sk_buff e acertado na volta (wlp_to_native).
 */
#include "wl_pkt.h"

/*
 * Espaco antes dos dados que o nucleo usa para os cabecalhos de TX
 * (802.11 + cabecalho do ucode + criptografia): o wlp_push nao realoca,
 * entao todo pacote que entra no nucleo tem de trazer pelo menos isto.
 */
#define WLP_TX_HEADROOM	256

static struct kmem_cache *wlp_cache;

int wlp_init(void)
{
	wlp_cache = kmem_cache_create("wl_pkt", sizeof(struct wl_pkt), 0,
				      SLAB_HWCACHE_ALIGN, NULL);
	return wlp_cache ? 0 : -ENOMEM;
}

void wlp_exit(void)
{
	kmem_cache_destroy(wlp_cache);
	wlp_cache = NULL;
}

/* imitacao nova para um sk_buff linear */
static struct wl_pkt *wlp_wrap(wl_native_skb *skb)
{
	struct wl_pkt *p = kmem_cache_zalloc(wlp_cache, GFP_ATOMIC);

	if (!p)
		return NULL;
	p->native = skb;
	p->magic = WL_PKT_MAGIC;
	p->head = skb->head;
	p->data = skb->data;
	p->tail = skb_tail_pointer(skb);
	p->end = skb_end_pointer(skb);
	p->len = skb->len;
	p->data_len = 0;
	p->priority = skb->priority;
	p->protocol = skb->protocol;
	p->dev = skb->dev;
	p->ip_summed = skb->ip_summed;
	p->mark = skb->mark;
	p->truesize = skb->truesize;
	refcount_set(&p->users, 1);
	return p;
}

/* acerta o sk_buff real com o estado da imitacao */
static void wlp_sync(struct wl_pkt *p)
{
	wl_native_skb *skb = p->native;

	if (WARN_ON_ONCE(p->data < skb->head || p->tail > skb_end_pointer(skb) ||
			 p->data + p->len != p->tail)) {
		/* estado incoerente: mantem o skb consistente, sem dados */
		skb->len = 0;
		skb_reset_tail_pointer(skb);
		return;
	}
	skb->data = p->data;
	skb->len = p->len;
	skb_set_tail_pointer(skb, p->len);
	skb->priority = p->priority;
	skb->mark = p->mark;
	if (p->dev)
		skb->dev = p->dev;
}

static void wlp_release(struct wl_pkt *p)
{
	p->magic = 0;
	kmem_cache_free(wlp_cache, p);
}

/* TX: pacote da pilha -> imitacao (o skb passa a ser do driver) */
struct wl_pkt *wlp_from_native(wl_native_skb *skb)
{
	struct wl_pkt *p;

	if (skb_linearize(skb))
		goto drop;
	if (skb_cow_head(skb, WLP_TX_HEADROOM))
		goto drop;
	skb_orphan(skb);
	p = wlp_wrap(skb);
	if (!p)
		goto drop;
	return p;
drop:
	dev_kfree_skb_any(skb);
	return NULL;
}

/* RX: imitacao -> pacote para a pilha (a imitacao e liberada) */
wl_native_skb *wlp_to_native(struct wl_pkt *p)
{
	wl_native_skb *skb;

	if (WARN_ON_ONCE(p->magic != WL_PKT_MAGIC))
		return NULL;
	skb = p->native;
	wlp_sync(p);
	wlp_release(p);
	return skb;
}

/* osl_alloc_skb(): buffer vazio, como dev_alloc_skb() */
struct wl_pkt *wlp_dev_alloc(unsigned int len)
{
	wl_native_skb *skb = dev_alloc_skb(len);
	struct wl_pkt *p;

	if (!skb)
		return NULL;
	p = wlp_wrap(skb);
	if (!p)
		dev_kfree_skb_any(skb);
	return p;
}

void wlp_free(struct wl_pkt *p)
{
	if (!p)
		return;
	if (WARN_ON_ONCE(p->magic != WL_PKT_MAGIC))
		return;
	dev_kfree_skb_any(p->native);
	wlp_release(p);
}

/* osl_pktdup(): copia inteira (o nucleo pode alterar as duas) */
struct wl_pkt *wlp_copy(struct wl_pkt *p)
{
	wl_native_skb *nskb;
	struct wl_pkt *np;

	wlp_sync(p);
	nskb = skb_copy_expand(p->native, skb_headroom(p->native),
			       skb_tailroom(p->native), GFP_ATOMIC);
	if (!nskb)
		return NULL;
	np = wlp_wrap(nskb);
	if (!np) {
		dev_kfree_skb_any(nskb);
		return NULL;
	}
	memcpy(np->cb, p->cb, sizeof(np->cb));
	np->priority = p->priority;
	return np;
}

/*
 * PKTEXPHEADROOM: devolve um pacote novo com pelo menos headroom bytes
 * antes dos dados; o original continua valido (como skb_realloc_headroom).
 */
struct wl_pkt *wlp_realloc_headroom(struct wl_pkt *p, unsigned int headroom)
{
	wl_native_skb *nskb;
	struct wl_pkt *np;

	wlp_sync(p);
	nskb = skb_realloc_headroom(p->native, headroom);
	if (!nskb)
		return NULL;
	np = wlp_wrap(nskb);
	if (!np) {
		dev_kfree_skb_any(nskb);
		return NULL;
	}
	memcpy(np->cb, p->cb, sizeof(np->cb));
	return np;
}

/* PKTPADTAILROOM: zera pad bytes depois dos dados (skb_pad) */
int wlp_pad(struct wl_pkt *p, int pad)
{
	if (pad <= 0)
		return 0;
	if (wlp_tailroom(p) < pad) {
		/* skb_pad libera o pacote se falhar */
		wlp_free(p);
		return -ENOMEM;
	}
	memset(p->tail, 0, pad);
	return 0;
}

void *wlp_push(struct wl_pkt *p, unsigned int n)
{
	if (WARN_ONCE(p->data - n < p->head, "wl_pkt: push %u sem headroom (%td)\n",
		      n, p->data - p->head))
		n = p->data - p->head;	/* evita escrever fora do buffer */
	p->data -= n;
	p->len += n;
	return p->data;
}

void *wlp_pull(struct wl_pkt *p, unsigned int n)
{
	if (n > p->len)
		return NULL;
	p->data += n;
	p->len -= n;
	return p->data;
}
