// SPDX-License-Identifier: GPL-2.0
/*
 * wl_linux.c - camada de SO para o nucleo binario do wl Broadcom
 * 7.14.164.18 no Linux 6.12 (OpenWrt bcm53xx, Netgear R7000).
 *
 * Reescrita a partir do contrato observado no wl_linux.o do DD-WRT;
 * a estrutura segue a do wl_linux.c do broadcom-sta.  O nucleo so ve
 * wl_info/wl_if como ponteiros opacos.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>

#include "wl_abi.h"

#define WL_DRV_NAME	"wl"
#define WL_VERSION_STR	"7.14.164.18 (r692288)"
#define WL_IOCTL_MAXLEN	8192
#define WL_IO_REGSIZE	0x4000

struct wl_timer {
	struct timer_list	timer;
	struct wl_info		*wl;
	void			(*fn)(void *arg);
	void			*arg;
	uint			ms;
	bool			periodic;
	bool			set;
	struct wl_timer		*next;
	const char		*name;
};

struct wl_if {
	struct wl_info		*wl;
	struct wlc_if		*wlcif;
	struct net_device	*dev;
	struct wl_if		*next;
	uint			unit;
	bool			registered;
	struct work_struct	reg_work;
	char			name[IFNAMSIZ];
};

struct wl_info {
	wlc_pub_t		*pub;
	struct wlc_info		*wlc;
	osl_t			*osh;
	struct pci_dev		*pdev;
	struct net_device	*dev;		/* interface primaria */
	struct wl_if		*iflist;
	struct wl_timer		*timers;
	void __iomem		*regsva;
	uint			unit;
	int			irq;
	bool			irq_ok;

	spinlock_t		lock;		/* WL_LOCK: contexto do nucleo */
	spinlock_t		isr_lock;	/* WL_ISRLOCK */
	struct tasklet_struct	dpc_tasklet;
	bool			resched;
	int			dpc_info;	/* wlc_dpc_info (processed) */
	struct mutex		perimeter;	/* ioctls que dormem */
};

static uint wl_units;
extern struct device *wl_dma_dev;	/* shim.c */

/*
 * O nucleo binario acessa sk_buff por offset fixo (ABI do kernel 4.4
 * do DD-WRT).  O kernel precisa do patch 990-skbuff-bcm-wl-abi.
 */
#define WL_SKB_AT(f, off) \
	static_assert(offsetof(struct sk_buff, f) == (off), \
		"sk_buff." #f " fora do offset do nucleo wl")
WL_SKB_AT(next, 0x00);
WL_SKB_AT(prev, 0x04);
WL_SKB_AT(pktc_cb, 0x10);
WL_SKB_AT(cb, 0x30);
WL_SKB_AT(ctfpool, 0x70);
WL_SKB_AT(pktc_flags, 0x74);
WL_SKB_AT(len, 0x94);
WL_SKB_AT(data_len, 0x98);
WL_SKB_AT(priority, 0xb4);
WL_SKB_AT(tail, 0xdc);
WL_SKB_AT(end, 0xe0);
WL_SKB_AT(head, 0xe4);
WL_SKB_AT(data, 0xe8);

#define WL_LOCK(wl)	spin_lock_bh(&(wl)->lock)
#define WL_UNLOCK(wl)	spin_unlock_bh(&(wl)->lock)

struct wl_netpriv {
	struct wl_if	*wlif;
};

static inline struct wl_if *dev_wlif(struct net_device *dev)
{
	return ((struct wl_netpriv *)netdev_priv(dev))->wlif;
}

/* ---------------- callbacks exigidos pelo nucleo ---------------- */

void wl_init(struct wl_info *wl)
{
	wl_reset(wl);
	wlc_init(wl->wlc);
}

uint wl_reset(struct wl_info *wl)
{
	wlc_reset(wl->wlc);
	wl->resched = false;
	return 0;
}

void wl_intrson(struct wl_info *wl)
{
	unsigned long flags;

	spin_lock_irqsave(&wl->isr_lock, flags);
	wlc_intrson(wl->wlc);
	spin_unlock_irqrestore(&wl->isr_lock, flags);
}

uint32 wl_intrsoff(struct wl_info *wl)
{
	unsigned long flags;
	uint32 status;

	spin_lock_irqsave(&wl->isr_lock, flags);
	status = wlc_intrsoff(wl->wlc);
	spin_unlock_irqrestore(&wl->isr_lock, flags);
	return status;
}

void wl_intrsrestore(struct wl_info *wl, uint32 macintmask)
{
	unsigned long flags;

	spin_lock_irqsave(&wl->isr_lock, flags);
	wlc_intrsrestore(wl->wlc, macintmask);
	spin_unlock_irqrestore(&wl->isr_lock, flags);
}

int wl_up(struct wl_info *wl)
{
	struct wl_if *wlif;
	int err;

	if (WLPUB_UP(wl->pub))
		return 0;

	err = wlc_up(wl->wlc);
	if (err)
		return err;

	for (wlif = wl->iflist; wlif; wlif = wlif->next)
		if (wlif->registered)
			netif_tx_wake_all_queues(wlif->dev);
	return 0;
}

void wl_down(struct wl_info *wl)
{
	struct wl_if *wlif;

	for (wlif = wl->iflist; wlif; wlif = wlif->next)
		if (wlif->registered)
			netif_tx_stop_all_queues(wlif->dev);

	wlc_down(wl->wlc);

	/*
	 * O DPC pode estar esperando o lock; ele ve pub->up == 0 e sai.
	 * Soltamos o lock so para deixa-lo terminar.
	 */
	WL_UNLOCK(wl);
	tasklet_kill(&wl->dpc_tasklet);
	WL_LOCK(wl);
}

void wl_event(struct wl_info *wl, char *ifname, wlc_event_t *e)
{
	/* TODO: repassar eventos (assoc, mic error...) ao userspace */
}

void wl_event_sync(struct wl_info *wl, char *ifname, wlc_event_t *e)
{
}

void wl_dump_ver(struct wl_info *wl, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "wl%d: r7000-wl version %s\n", WLPUB_UNIT(wl->pub),
		WL_VERSION_STR);
}

void wl_txflowcontrol(struct wl_info *wl, struct wl_if *wlif, bool state,
	int prio)
{
	struct net_device *dev = wlif ? wlif->dev : wl->dev;

	if (wlif && !wlif->registered)
		return;
	if (state)
		netif_tx_stop_all_queues(dev);
	else
		netif_tx_wake_all_queues(dev);
}

bool wl_alloc_dma_resources(struct wl_info *wl, uint dmaddrwidth)
{
	/* igual ao original: nada a fazer, a mascara e acertada no probe */
	return TRUE;
}

void *wl_get_wireless_stats(void *dev)
{
	return NULL;
}

int wl_osl_pcie_rc(struct wl_info *wl, uint op, int param)
{
	/* TODO: operacoes no root complex (PERST etc.) usadas por WARs */
	pr_debug("wl%d: wl_osl_pcie_rc(op=%u, param=%d) ignorado\n",
		wl->unit, op, param);
	return 0;
}

void wl_monitor(struct wl_info *wl, wl_rxsts_t *rxsts, void *p)
{
	PKTFREE(wl->osh, p, FALSE);
}

void wl_set_monitor(struct wl_info *wl, int val)
{
}

void *wl_get_ifctx(struct wl_info *wl, int ctx_id, struct wl_if *wlif)
{
	if (ctx_id != IFCTX_NETDEV)
		return NULL;
	return wlif ? wlif->dev : wl->dev;
}

char *wl_ifname(struct wl_info *wl, struct wl_if *wlif)
{
	if (wlif)
		return wlif->name;
	return wl->dev->name;
}

/* ---------------- timers ---------------- */

static void wl_timer_cb(struct timer_list *t)
{
	struct wl_timer *wt = from_timer(wt, t, timer);
	struct wl_info *wl = wt->wl;

	WL_LOCK(wl);
	if (wt->set) {
		if (wt->periodic)
			mod_timer(&wt->timer, jiffies + msecs_to_jiffies(wt->ms));
		else
			wt->set = false;
		wt->fn(wt->arg);
	}
	WL_UNLOCK(wl);
}

struct wl_timer *wl_init_timer(struct wl_info *wl, void (*fn)(void *arg),
	void *arg, const char *name)
{
	struct wl_timer *wt;

	wt = kzalloc(sizeof(*wt), GFP_ATOMIC);
	if (!wt)
		return NULL;

	timer_setup(&wt->timer, wl_timer_cb, 0);
	wt->wl = wl;
	wt->fn = fn;
	wt->arg = arg;
	wt->name = name;
	wt->next = wl->timers;
	wl->timers = wt;
	return wt;
}

void wl_add_timer(struct wl_info *wl, struct wl_timer *wt, uint ms, int periodic)
{
	wt->ms = ms;
	wt->periodic = periodic;
	wt->set = true;
	mod_timer(&wt->timer, jiffies + msecs_to_jiffies(ms));
}

bool wl_del_timer(struct wl_info *wl, struct wl_timer *wt)
{
	if (wt->set) {
		wt->set = false;
		/* chamado com WL_LOCK: nao pode esperar o callback */
		if (!timer_delete(&wt->timer))
			return FALSE;
	}
	return TRUE;
}

void wl_free_timer(struct wl_info *wl, struct wl_timer *wt)
{
	struct wl_timer **pp;

	for (pp = &wl->timers; *pp; pp = &(*pp)->next) {
		if (*pp == wt) {
			*pp = wt->next;
			break;
		}
	}
	timer_delete_sync(&wt->timer);
	kfree(wt);
}

/* ---------------- rx / tx ---------------- */

void wl_sendup(struct wl_info *wl, struct wl_if *wlif, void *p, int numpkt)
{
	struct net_device *dev = wlif ? wlif->dev : wl->dev;
	struct sk_buff *skb;

	if (wlif && !wlif->registered) {
		PKTFREE(wl->osh, p, FALSE);
		return;
	}

	skb = PKTTONATIVE(wl->osh, p);
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;
	netif_rx(skb);
}

static netdev_tx_t wl_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct wl_if *wlif = dev_wlif(dev);
	struct wl_info *wl = wlif->wl;
	void *pkt;

	WL_LOCK(wl);
	pkt = PKTFRMNATIVE(wl->osh, skb);
	if (!pkt) {
		WL_UNLOCK(wl);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}
	pktsetprio(pkt, FALSE);
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	/* o nucleo e dono do pacote a partir daqui, enviado ou nao */
	wlc_sendpkt(wl->wlc, pkt, wlif->wlcif);
	WL_UNLOCK(wl);
	return NETDEV_TX_OK;
}

/* ---------------- isr / dpc ---------------- */

static irqreturn_t wl_isr(int irq, void *dev_id)
{
	struct wl_info *wl = dev_id;
	bool ours, wantdpc = FALSE;

	spin_lock(&wl->isr_lock);
	ours = wlc_isr(wl->wlc, &wantdpc);
	if (ours && wantdpc)
		tasklet_schedule(&wl->dpc_tasklet);
	spin_unlock(&wl->isr_lock);

	return IRQ_RETVAL(ours);
}

static void wl_dpc(struct tasklet_struct *t)
{
	struct wl_info *wl = from_tasklet(wl, t, dpc_tasklet);
	unsigned long flags;

	WL_LOCK(wl);
	if (!WLPUB_UP(wl->pub))
		goto done;

	if (wl->resched) {
		spin_lock_irqsave(&wl->isr_lock, flags);
		wlc_intrsupd(wl->wlc);
		spin_unlock_irqrestore(&wl->isr_lock, flags);
	}

	wl->dpc_info = 0;
	wl->resched = wlc_dpc(wl->wlc, TRUE, &wl->dpc_info);

	if (!WLPUB_UP(wl->pub))
		goto done;

	if (wl->resched) {
		tasklet_schedule(&wl->dpc_tasklet);
	} else {
		spin_lock_irqsave(&wl->isr_lock, flags);
		wlc_intrson(wl->wlc);
		spin_unlock_irqrestore(&wl->isr_lock, flags);
	}
done:
	WL_UNLOCK(wl);
}

/* ---------------- net_device ---------------- */

static int wl_open(struct net_device *dev)
{
	struct wl_info *wl = dev_wlif(dev)->wl;
	int err;

	WL_LOCK(wl);
	err = wl_up(wl);
	if (!err)
		wlc_set(wl->wlc, WLC_SET_PROMISC_ABI, !!(dev->flags & IFF_PROMISC));
	WL_UNLOCK(wl);

	return err ? -ENODEV : 0;
}

static int wl_close(struct net_device *dev)
{
	struct wl_if *wlif = dev_wlif(dev);
	struct wl_info *wl = wlif->wl;

	/* so a interface primaria derruba o radio */
	if (dev != wl->dev)
		return 0;

	WL_LOCK(wl);
	wl_down(wl);
	WL_UNLOCK(wl);
	return 0;
}

static int wl_set_mac_address(struct net_device *dev, void *addr)
{
	struct wl_if *wlif = dev_wlif(dev);
	struct wl_info *wl = wlif->wl;
	struct sockaddr *sa = addr;
	int err;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	WL_LOCK(wl);
	err = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, sa->sa_data,
		ETH_ALEN, TRUE, wlif->wlcif);
	WL_UNLOCK(wl);
	if (err)
		return -EINVAL;

	eth_hw_addr_set(dev, sa->sa_data);
	return 0;
}

static void wl_set_rx_mode(struct net_device *dev)
{
	struct wl_if *wlif = dev_wlif(dev);
	struct wl_info *wl = wlif->wl;

	/* TODO: mcast_list; por ora aceita todo multicast */
	WL_LOCK(wl);
	if (WLPUB_UP(wl->pub)) {
		wlc_iovar_setint(wl->wlc, "allmulti", 1);
		wlc_set(wl->wlc, WLC_SET_PROMISC_ABI, !!(dev->flags & IFF_PROMISC));
	}
	WL_UNLOCK(wl);
}

/*
 * SIOCDEVPRIVATE: interface do utilitario `wl` (wl_ioctl_t).
 * O buffer vai para o kernel antes de pegar o lock.
 */
static int wl_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
	void __user *data, int cmd)
{
	struct wl_if *wlif = dev_wlif(dev);
	struct wl_info *wl = wlif->wl;
	wl_ioctl_t ioc;
	void *buf = NULL;
	int err, bcmerr;

	if (cmd != SIOCDEVPRIVATE)
		return -EOPNOTSUPP;
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	if (copy_from_user(&ioc, data, sizeof(ioc)))
		return -EFAULT;
	if (ioc.len > WL_IOCTL_MAXLEN)
		ioc.len = WL_IOCTL_MAXLEN;

	if (ioc.buf && ioc.len) {
		buf = kmalloc(ioc.len, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		if (copy_from_user(buf, (void __user *)ioc.buf, ioc.len)) {
			kfree(buf);
			return -EFAULT;
		}
	}

	mutex_lock(&wl->perimeter);
	WL_LOCK(wl);
	bcmerr = wlc_ioctl(wl->wlc, ioc.cmd, buf, ioc.len, wlif->wlcif);
	WL_UNLOCK(wl);
	mutex_unlock(&wl->perimeter);

	err = bcmerr ? -EINVAL : 0;
	if (!bcmerr && buf && copy_to_user((void __user *)ioc.buf, buf, ioc.len))
		err = -EFAULT;
	kfree(buf);
	return err;
}

static const struct net_device_ops wl_netdev_ops = {
	.ndo_open		= wl_open,
	.ndo_stop		= wl_close,
	.ndo_start_xmit		= wl_start_xmit,
	.ndo_set_mac_address	= wl_set_mac_address,
	.ndo_set_rx_mode	= wl_set_rx_mode,
	.ndo_siocdevprivate	= wl_siocdevprivate,
};

/* o utilitario `wl` reconhece a interface pelo driver "wl" no ethtool */
static void wl_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strscpy(info->driver, WL_DRV_NAME, sizeof(info->driver));
	strscpy(info->version, WL_VERSION_STR, sizeof(info->version));
}

static const struct ethtool_ops wl_ethtool_ops = {
	.get_drvinfo	= wl_get_drvinfo,
};

static struct wl_if *wl_alloc_if(struct wl_info *wl, struct wlc_if *wlcif,
	uint unit, const char *fmt)
{
	struct net_device *dev;
	struct wl_if *wlif;

	dev = alloc_etherdev(sizeof(struct wl_netpriv));
	if (!dev)
		return NULL;

	wlif = kzalloc(sizeof(*wlif), GFP_ATOMIC);
	if (!wlif) {
		free_netdev(dev);
		return NULL;
	}

	wlif->wl = wl;
	wlif->wlcif = wlcif;
	wlif->dev = dev;
	wlif->unit = unit;
	((struct wl_netpriv *)netdev_priv(dev))->wlif = wlif;

	dev->netdev_ops = &wl_netdev_ops;
	dev->ethtool_ops = &wl_ethtool_ops;
	SET_NETDEV_DEV(dev, &wl->pdev->dev);
	eth_hw_addr_set(dev, WLPUB_CUR_ETHERADDR(wl->pub));
	snprintf(dev->name, IFNAMSIZ, fmt, wl->unit, unit);
	strscpy(wlif->name, dev->name, sizeof(wlif->name));

	wlif->next = wl->iflist;
	wl->iflist = wlif;
	return wlif;
}

static void wl_unlink_if(struct wl_info *wl, struct wl_if *wlif)
{
	struct wl_if **pp;

	for (pp = &wl->iflist; *pp; pp = &(*pp)->next) {
		if (*pp == wlif) {
			*pp = wlif->next;
			return;
		}
	}
}

/* interfaces virtuais (BSS extra, WDS): registro fora do WL_LOCK */
static void wl_reg_work(struct work_struct *work)
{
	struct wl_if *wlif = container_of(work, struct wl_if, reg_work);

	if (register_netdev(wlif->dev) == 0)
		wlif->registered = true;
	else
		pr_err("%s: register_netdev falhou\n", wlif->name);
}

struct wl_if *wl_add_if(struct wl_info *wl, struct wlc_if *wlcif, uint unit,
	struct ether_addr *remote)
{
	struct wl_if *wlif;

	wlif = wl_alloc_if(wl, wlcif, unit, remote ? "wds%d.%d" : "wl%d.%d");
	if (!wlif)
		return NULL;
	INIT_WORK(&wlif->reg_work, wl_reg_work);
	schedule_work(&wlif->reg_work);
	return wlif;
}

void wl_del_if(struct wl_info *wl, struct wl_if *wlif)
{
	wl_unlink_if(wl, wlif);
	/*
	 * chamado com WL_LOCK; o unregister dorme.  TODO: adiar para um
	 * work como no original.  Por enquanto so a primaria existe.
	 */
	wlif->wlcif = NULL;
}

/* ---------------- probe / remove ---------------- */

static void wl_linux_watchdog(void *ctx)
{
}

static void wl_free(struct wl_info *wl)
{
	struct wl_if *wlif;

	if (wl->irq_ok)
		free_irq(wl->irq, wl);
	tasklet_kill(&wl->dpc_tasklet);

	while ((wlif = wl->iflist)) {
		wl->iflist = wlif->next;
		if (wlif->registered)
			unregister_netdev(wlif->dev);
		free_netdev(wlif->dev);
		kfree(wlif);
	}

	if (wl->wlc) {
		wlc_module_unregister(wl->pub, "linux", wl);
		wlc_detach(wl->wlc);
	}
	while (wl->timers)
		wl_free_timer(wl, wl->timers);
	if (wl->regsva)
		iounmap(wl->regsva);
	if (wl->osh)
		osl_detach(wl->osh);
	kfree(wl);
}

static int wl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct wl_info *wl;
	struct wl_if *wlif;
	uint err = 0;
	u32 val;
	int ret;

	if (!wlc_chipmatch(pdev->vendor, pdev->device))
		return -ENODEV;

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	pci_set_master(pdev);

	/* desliga o retry timeout (0x41), como o original */
	pci_read_config_dword(pdev, 0x40, &val);
	if (val & 0x0000ff00)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		goto err_disable;

	wl = kzalloc(sizeof(*wl), GFP_KERNEL);
	if (!wl) {
		ret = -ENOMEM;
		goto err_disable;
	}

	dev_info(&pdev->dev, "wl%u: dominio PCI %d, slot %u\n", wl_units,
		pci_domain_nr(pdev->bus), PCI_SLOT(pdev->devfn));

	wl->pdev = pdev;
	wl->unit = wl_units;
	wl->irq = pdev->irq;
	spin_lock_init(&wl->lock);
	spin_lock_init(&wl->isr_lock);
	mutex_init(&wl->perimeter);
	tasklet_setup(&wl->dpc_tasklet, wl_dpc);

	wl->osh = osl_attach(pdev, PCI_BUS, TRUE);
	if (!wl->osh) {
		ret = -ENOMEM;
		goto err_free;
	}

	wl->regsva = ioremap(pci_resource_start(pdev, 0), WL_IO_REGSIZE);
	if (!wl->regsva) {
		ret = -ENOMEM;
		goto err_free;
	}

	wl->wlc = wlc_attach(wl, pdev->vendor, pdev->device, wl->unit, FALSE,
		wl->osh, (void __force *)wl->regsva, PCI_BUS, pdev, NULL, &err);
	if (!wl->wlc) {
		dev_err(&pdev->dev, "wl%d: wlc_attach falhou (err %u)\n",
			wl->unit, err);
		ret = -ENODEV;
		goto err_free;
	}
	wl->pub = wlc_pub(wl->wlc);

	wlif = wl_alloc_if(wl, wlc_wlcif_get_by_index(wl->wlc, 0), 0, "wl%d");
	if (!wlif) {
		ret = -ENOMEM;
		goto err_free;
	}
	/* wl_alloc_if usou "wl%d" com unit do radio */
	wl->dev = wlif->dev;

	wlc_module_register(wl->pub, NULL, "linux", wl, NULL,
		wl_linux_watchdog, NULL, NULL);

	ret = request_irq(wl->irq, wl_isr, IRQF_SHARED, wlif->name, wl);
	if (ret) {
		dev_err(&pdev->dev, "wl%d: request_irq(%d) falhou\n",
			wl->unit, wl->irq);
		goto err_free;
	}
	wl->irq_ok = true;

	ret = register_netdev(wl->dev);
	if (ret)
		goto err_free;
	wlif->registered = true;

	pci_set_drvdata(pdev, wl);
	if (!wl_dma_dev)
		wl_dma_dev = &pdev->dev;
	wl_units++;

	netdev_info(wl->dev, "Broadcom BCM%04x 802.11 Wireless Controller %s\n",
		pdev->device, WL_VERSION_STR);
	return 0;

err_free:
	wl_free(wl);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void wl_pci_remove(struct pci_dev *pdev)
{
	struct wl_info *wl = pci_get_drvdata(pdev);

	if (!wl)
		return;

	WL_LOCK(wl);
	if (WLPUB_UP(wl->pub))
		wl_down(wl);
	WL_UNLOCK(wl);

	wl_free(wl);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}

static const struct pci_device_id wl_id_table[] = {
	{ PCI_DEVICE(0x14e4, 0x43a0) },	/* BCM4360 */
	{ PCI_DEVICE(0x14e4, 0x43a1) },
	{ PCI_DEVICE(0x14e4, 0x43a2) },
	{ PCI_DEVICE(0x14e4, 0x4360) },
	{ }
};
MODULE_DEVICE_TABLE(pci, wl_id_table);

static struct pci_driver wl_pci_driver = {
	.name		= WL_DRV_NAME,
	.id_table	= wl_id_table,
	.probe		= wl_pci_probe,
	.remove		= wl_pci_remove,
};

extern void wl_nvram_free(void);

static int __init wl_module_init(void)
{
	return pci_register_driver(&wl_pci_driver);
}

static void __exit wl_module_exit(void)
{
	pci_unregister_driver(&wl_pci_driver);
	wl_nvram_free();
}

module_init(wl_module_init);
module_exit(wl_module_exit);

MODULE_DESCRIPTION("Broadcom wl (nucleo 7.14) para OpenWrt/R7000");
MODULE_LICENSE("Proprietary");
