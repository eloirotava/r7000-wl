// SPDX-License-Identifier: GPL-2.0
/*
 * wltrace: grava os acessos do codigo PHY do wl (registradores do PHY e do
 * radio, tabelas, delays, PMU, shm) para reproduzir o init do AC-PHY no b43.
 *
 * scripts/prep-core.sh (WLTRACE=1) troca, nos objetos de wl/phy, as
 * chamadas a esses acessores por wltrace_<nome>.  Cada wrapper grava um
 * registro e chama o original.
 *
 * Registro: 4 x u32 = { tipo | unidade << 8 | seq << 12, a, b, c }.
 * Tabelas: registro T seguido de ceil(bytes/12) registros D com os dados.
 * /proc/wltrace: ler = buffer binario; escrever 0 pausa, 1 liga, c limpa.
 */
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

enum {
	WT_PHY_W = 1, WT_PHY_R, WT_PHY_MOD, WT_PHY_AND, WT_PHY_OR,
	WT_RAD_W, WT_RAD_R, WT_RAD_MOD, WT_RAD_OR,
	WT_TBL_W, WT_TBL_R, WT_DATA, WT_DELAY, WT_PMU, WT_COREREG,
	WT_SHM_W, WT_MHF, WT_EVENT,
};

enum { EV_SUSPEND = 1, EV_ENABLE, EV_PHYCLK_FGC, EV_BW_SET, EV_MACFREQ };

#define WT_BYTES	(8 << 20)
#define WT_RECS		(WT_BYTES / 16)

static u32 *wt_buf;
static unsigned int wt_n;
static bool wt_on = true;
static DEFINE_SPINLOCK(wt_lock);

/* ate 4 phy_info distintos (dois radios + folga) viram unidade 0..3 */
static void *wt_pi[4];

static unsigned int wt_unit(void *pi)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wt_pi); i++) {
		if (wt_pi[i] == pi)
			return i;
		if (!wt_pi[i]) {
			wt_pi[i] = pi;
			return i;
		}
	}
	return 0xf;
}

static void wt_put_locked(u32 type, void *pi, u32 a, u32 b, u32 c)
{
	u32 *r;

	if (!wt_buf || !wt_on || wt_n >= WT_RECS)
		return;
	r = &wt_buf[wt_n * 4];
	r[0] = type | (wt_unit(pi) << 8) | (wt_n << 12);
	r[1] = a;
	r[2] = b;
	r[3] = c;
	wt_n++;
}

/* primeiro uso de cada tipo vai para o printk (netconsole): se travar,
 * mostra ate onde chegou */
static unsigned long wt_seen;

static void wt_first(u32 type, u32 a, u32 b)
{
	if (!test_and_set_bit(type, &wt_seen))
		pr_info("wltrace: 1o tipo %u (%#x %#x)\n", type, a, b);
}

static void wt_put(u32 type, void *pi, u32 a, u32 b, u32 c)
{
	unsigned long flags;

	wt_first(type, a, b);
	spin_lock_irqsave(&wt_lock, flags);
	wt_put_locked(type, pi, a, b, c);
	spin_unlock_irqrestore(&wt_lock, flags);
}

/* phytbl_info_t do wl (mesmo layout do brcmsmac, conferido no
 * acphytbl_info_rev0: 23 entradas de 20 bytes) */
struct wt_phytbl {
	const void *ptr;
	u32 len;
	u32 id;
	u32 offset;
	u32 width;
};

static void wt_table(u32 type, void *pi, const struct wt_phytbl *t)
{
	unsigned long flags;
	u32 bytes, i, w[3];
	const u8 *p = t->ptr;

	wt_first(type, t->id, t->len);
	bytes = t->len * (t->width / 8);
	if (bytes > 65536 || (t->width != 8 && t->width != 16 &&
			      t->width != 32 && t->width != 48 && t->width != 64)) {
		static bool warned;

		if (!warned)
			pr_info("wltrace: tabela estranha id %#x len %u width %u ptr %px\n",
				t->id, t->len, t->width, t->ptr);
		warned = true;
		bytes = 0;
	}
	spin_lock_irqsave(&wt_lock, flags);
	wt_put_locked(type, pi, t->id, t->offset, t->len | (t->width << 24));
	if (type == WT_TBL_W && p) {
		for (i = 0; i < bytes && wt_n < WT_RECS; i += 12) {
			memset(w, 0, sizeof(w));
			memcpy(w, p + i, min_t(u32, 12, bytes - i));
			wt_put_locked(WT_DATA, pi, w[0], w[1], w[2]);
		}
	}
	spin_unlock_irqrestore(&wt_lock, flags);
}

/* originais (assinaturas do wl; todos os argumentos cabem em registradores) */
extern void phy_reg_write(void *pi, u32 addr, u32 val);
extern u32 phy_reg_read(void *pi, u32 addr);
extern void phy_reg_mod(void *pi, u32 addr, u32 mask, u32 val);
extern void phy_reg_and(void *pi, u32 addr, u32 val);
extern void phy_reg_or(void *pi, u32 addr, u32 val);
extern void write_radio_reg(void *pi, u32 addr, u32 val);
extern u32 read_radio_reg(void *pi, u32 addr);
extern void mod_radio_reg(void *pi, u32 addr, u32 mask, u32 val);
extern void or_radio_reg(void *pi, u32 addr, u32 val);
extern void wlc_phy_write_table_ext(void *pi, const struct wt_phytbl *t);
extern void wlc_phy_read_table_ext(void *pi, const struct wt_phytbl *t);
extern void osl_delay(u32 us);
extern u32 si_pmu_regcontrol(void *sih, u32 reg, u32 mask, u32 val);
extern u32 si_corereg(void *sih, u32 coreidx, u32 off, u32 mask, u32 val);
extern void wlapi_bmac_write_shm(void *sh, u32 off, u32 val);
extern void wlapi_bmac_mhf(void *sh, u32 idx, u32 mask, u32 val, u32 bands);
extern void wlapi_suspend_mac_and_wait(void *sh);
extern void wlapi_enable_mac(void *sh);
extern void wlapi_bmac_phyclk_fgc(void *sh, u32 clk);
extern void wlapi_bmac_bw_set(void *sh, u32 bw);
extern void wlapi_switch_macfreq(void *sh, u32 spurmode);

void wltrace_phy_reg_write(void *pi, u32 addr, u32 val)
{
	wt_put(WT_PHY_W, pi, addr, val, 0);
	phy_reg_write(pi, addr, val);
}

u32 wltrace_phy_reg_read(void *pi, u32 addr)
{
	u32 v = phy_reg_read(pi, addr);

	wt_put(WT_PHY_R, pi, addr, v, 0);
	return v;
}

void wltrace_phy_reg_mod(void *pi, u32 addr, u32 mask, u32 val)
{
	wt_put(WT_PHY_MOD, pi, addr, mask, val);
	phy_reg_mod(pi, addr, mask, val);
}

void wltrace_phy_reg_and(void *pi, u32 addr, u32 val)
{
	wt_put(WT_PHY_AND, pi, addr, val, 0);
	phy_reg_and(pi, addr, val);
}

void wltrace_phy_reg_or(void *pi, u32 addr, u32 val)
{
	wt_put(WT_PHY_OR, pi, addr, val, 0);
	phy_reg_or(pi, addr, val);
}

void wltrace_write_radio_reg(void *pi, u32 addr, u32 val)
{
	wt_put(WT_RAD_W, pi, addr, val, 0);
	write_radio_reg(pi, addr, val);
}

u32 wltrace_read_radio_reg(void *pi, u32 addr)
{
	u32 v = read_radio_reg(pi, addr);

	wt_put(WT_RAD_R, pi, addr, v, 0);
	return v;
}

void wltrace_mod_radio_reg(void *pi, u32 addr, u32 mask, u32 val)
{
	wt_put(WT_RAD_MOD, pi, addr, mask, val);
	mod_radio_reg(pi, addr, mask, val);
}

void wltrace_or_radio_reg(void *pi, u32 addr, u32 val)
{
	wt_put(WT_RAD_OR, pi, addr, val, 0);
	or_radio_reg(pi, addr, val);
}

void wltrace_wlc_phy_write_table_ext(void *pi, const struct wt_phytbl *t)
{
	wt_table(WT_TBL_W, pi, t);
	wlc_phy_write_table_ext(pi, t);
}

void wltrace_wlc_phy_read_table_ext(void *pi, const struct wt_phytbl *t)
{
	wt_table(WT_TBL_R, pi, t);
	wlc_phy_read_table_ext(pi, t);
}

void wltrace_osl_delay(u32 us)
{
	wt_put(WT_DELAY, NULL, us, 0, 0);
	osl_delay(us);
}

u32 wltrace_si_pmu_regcontrol(void *sih, u32 reg, u32 mask, u32 val)
{
	u32 r = si_pmu_regcontrol(sih, reg, mask, val);

	wt_put(WT_PMU, NULL, reg, mask, val);
	return r;
}

u32 wltrace_si_corereg(void *sih, u32 coreidx, u32 off, u32 mask, u32 val)
{
	u32 r = si_corereg(sih, coreidx, off, mask, val);

	wt_put(WT_COREREG, NULL, coreidx << 16 | off, mask, val);
	return r;
}

void wltrace_wlapi_bmac_write_shm(void *sh, u32 off, u32 val)
{
	wt_put(WT_SHM_W, NULL, off, val, 0);
	wlapi_bmac_write_shm(sh, off, val);
}

void wltrace_wlapi_bmac_mhf(void *sh, u32 idx, u32 mask, u32 val, u32 bands)
{
	wt_put(WT_MHF, NULL, idx, mask, val);
	wlapi_bmac_mhf(sh, idx, mask, val, bands);
}

void wltrace_wlapi_suspend_mac_and_wait(void *sh)
{
	wt_put(WT_EVENT, NULL, EV_SUSPEND, 0, 0);
	wlapi_suspend_mac_and_wait(sh);
}

void wltrace_wlapi_enable_mac(void *sh)
{
	wt_put(WT_EVENT, NULL, EV_ENABLE, 0, 0);
	wlapi_enable_mac(sh);
}

void wltrace_wlapi_bmac_phyclk_fgc(void *sh, u32 clk)
{
	wt_put(WT_EVENT, NULL, EV_PHYCLK_FGC, clk, 0);
	wlapi_bmac_phyclk_fgc(sh, clk);
}

void wltrace_wlapi_bmac_bw_set(void *sh, u32 bw)
{
	wt_put(WT_EVENT, NULL, EV_BW_SET, bw, 0);
	wlapi_bmac_bw_set(sh, bw);
}

void wltrace_wlapi_switch_macfreq(void *sh, u32 spurmode)
{
	wt_put(WT_EVENT, NULL, EV_MACFREQ, spurmode, 0);
	wlapi_switch_macfreq(sh, spurmode);
}

static ssize_t wt_read(struct file *f, char __user *ubuf, size_t len,
		       loff_t *pos)
{
	size_t total = (size_t)wt_n * 16;

	if (!wt_buf || *pos >= total)
		return 0;
	len = min_t(size_t, len, total - *pos);
	if (copy_to_user(ubuf, (u8 *)wt_buf + *pos, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t wt_write(struct file *f, const char __user *ubuf, size_t len,
			loff_t *pos)
{
	char c;
	unsigned long flags;

	if (!len || get_user(c, ubuf))
		return -EFAULT;
	spin_lock_irqsave(&wt_lock, flags);
	if (c == '0')
		wt_on = false;
	else if (c == '1')
		wt_on = true;
	else if (c == 'c')
		wt_n = 0;
	spin_unlock_irqrestore(&wt_lock, flags);
	return len;
}

static const struct proc_ops wt_ops = {
	.proc_read = wt_read,
	.proc_write = wt_write,
	.proc_lseek = default_llseek,
};

void wltrace_init(void)
{
	wt_buf = vmalloc(WT_BYTES);
	if (!wt_buf)
		return;
	proc_create("wltrace", 0600, NULL, &wt_ops);
	pr_info("wltrace: buffer de %u registros\n", WT_RECS);
}

void wltrace_exit(void)
{
	remove_proc_entry("wltrace", NULL);
	vfree(wt_buf);
	wt_buf = NULL;
}
