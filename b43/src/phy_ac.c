// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Broadcom B43 wireless driver
 * IEEE 802.11ac AC-PHY support
 *
 * Copyright (c) 2015 Rafał Miłecki <zajec5@gmail.com>
 */

#include "b43.h"
#include "main.h"
#include "phy_ac.h"
#include "phy_ac_replay.h"
#include "radio_2069.h"

/**************************************************
 * Basic PHY ops
 **************************************************/

static int b43_phy_ac_op_allocate(struct b43_wldev *dev)
{
	struct b43_phy_ac *phy_ac;

	phy_ac = kzalloc(sizeof(*phy_ac), GFP_KERNEL);
	if (!phy_ac)
		return -ENOMEM;
	dev->phy.ac = phy_ac;

	return 0;
}

static void b43_phy_ac_op_free(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_ac *phy_ac = phy->ac;

	kfree(phy_ac);
	phy->ac = NULL;
}

static void b43_phy_ac_op_maskset(struct b43_wldev *dev, u16 reg, u16 mask,
				  u16 set)
{
	b43_write16f(dev, B43_MMIO_PHY_CONTROL, reg);
	b43_write16(dev, B43_MMIO_PHY_DATA,
		    (b43_read16(dev, B43_MMIO_PHY_DATA) & mask) | set);
}

static u16 b43_phy_ac_op_radio_read(struct b43_wldev *dev, u16 reg)
{
	b43_write16f(dev, B43_MMIO_RADIO24_CONTROL, reg);
	return b43_read16(dev, B43_MMIO_RADIO24_DATA);
}

static void b43_phy_ac_op_radio_write(struct b43_wldev *dev, u16 reg,
				      u16 value)
{
	b43_write16f(dev, B43_MMIO_RADIO24_CONTROL, reg);
	b43_write16(dev, B43_MMIO_RADIO24_DATA, value);
}

static void b43_phy_ac_op_prepare_structs(struct b43_wldev *dev)
{
	memset(dev->phy.ac, 0, sizeof(*dev->phy.ac));
}

/**************************************************
 * Init gravado do wl (phy_ac_replay.c)
 **************************************************/

/* depuracao no R7000: quantas ops do replay rodar (-1 = todas) e intervalo
 * de checkpoints (mensagem + 1,5 s de espera para o log chegar ao flash) */
static int ac_replay_ops = -1;
module_param(ac_replay_ops, int, 0444);
static int ac_replay_ckpt;
module_param(ac_replay_ckpt, int, 0444);
static int ac_replay_skip = -1;
module_param(ac_replay_skip, int, 0444);

static void b43_phy_ac_ckpt(struct b43_wldev *dev, unsigned int i)
{
	const struct b43_ac_replay_op *op = &b43_ac_replay_ops[i];

	b43info(dev->wl, "replay: op %u tipo %u addr 0x%x\n", i, op->type,
		op->addr);
	mdelay(1500);
}

static void b43_phy_ac_delay(unsigned int us)
{
	if (us < 10)
		udelay(us);
	else
		usleep_range(us, us + us / 4 + 1);
}

/* tabela do AC-PHY pelos registradores 0x00d/0x00e/0x00f-0x011, como o wl
 * (argumentos extras de wlc_phy_write_table_ext no wltrace) */
static void b43_phy_ac_table_write(struct b43_wldev *dev, u16 id, u16 offset,
				   const struct b43_ac_replay_tbl *t)
{
	const u32 *d = &b43_ac_replay_data[t->start];
	unsigned int i;

	b43_phy_write(dev, B43_PHY_AC_TABLE_ID, id);
	b43_phy_write(dev, B43_PHY_AC_TABLE_OFFSET, offset);
	for (i = 0; i < t->len; i++) {
		switch (t->width) {
		case 48:
			/* como o wl: seleciona 0x011 uma vez e escreve as tres
			 * palavras (15:0, 31:16, 47:32) seguidas no PHY_DATA */
			b43_write16f(dev, B43_MMIO_PHY_CONTROL,
				     B43_PHY_AC_TABLE_DATA3);
			b43_write16(dev, B43_MMIO_PHY_DATA, d[3 * i]);
			b43_write16(dev, B43_MMIO_PHY_DATA, d[3 * i + 1]);
			b43_write16(dev, B43_MMIO_PHY_DATA, d[3 * i + 2]);
			break;
		case 32:
			b43_phy_write(dev, B43_PHY_AC_TABLE_DATA2, d[i] >> 16);
			b43_phy_write(dev, B43_PHY_AC_TABLE_DATA1, d[i] & 0xffff);
			break;
		default:
			b43_phy_write(dev, B43_PHY_AC_TABLE_DATA1, d[i]);
		}
	}
}

/* chipcommon/PMU do cartao (mesma lista do /proc/wlshm do wl) */
static void b43_phy_ac_pmu_dump(struct b43_wldev *dev, const char *quando)
{
	struct bcma_drv_cc *cc = &dev->dev->bdev->bus->drv_cc;
	static const u16 dir[] = { 0x28, 0x2c, 0x600, 0x604, 0x608, 0x60c,
				   0x610, 0x618, 0x61c };
	u32 v[49];
	unsigned int i, n = 0;

	for (i = 0; i < ARRAY_SIZE(dir); i++)
		v[n++] = bcma_cc_read32(cc, dir[i]);
	for (i = 0; i < 8; i++) {
		bcma_cc_write32(cc, 0x650, i);
		v[n++] = bcma_cc_read32(cc, 0x654);
	}
	for (i = 0; i < 16; i++) {
		bcma_cc_write32(cc, 0x658, i);
		v[n++] = bcma_cc_read32(cc, 0x65c);
	}
	for (i = 0; i < 16; i++) {
		bcma_cc_write32(cc, 0x660, i);
		v[n++] = bcma_cc_read32(cc, 0x664);
	}
	for (i = 0; i < n; i += 8)
		b43info(dev->wl, "pmu %s %02u: %*ph\n", quando, i,
			(int)(min(8u, n - i) * 4), &v[i]);
}

static void b43_phy_ac_replay(struct b43_wldev *dev)
{
	struct bcma_drv_cc *cc = &dev->dev->bdev->bus->drv_cc;
	const struct b43_ac_replay_op *op;
	const struct b43_ac_replay_pmu *pmu;
	unsigned int i;

	for (i = 0; i < b43_ac_replay_nops; i++) {
		if (ac_replay_ops >= 0 && i >= ac_replay_ops)
			break;
		if (ac_replay_ckpt > 0 && i % ac_replay_ckpt == 0)
			b43_phy_ac_ckpt(dev, i);
		if (i == ac_replay_skip)
			continue;
		op = &b43_ac_replay_ops[i];
		switch (op->type) {
		case B43_AC_OP_PHY_W:
			b43_phy_write(dev, op->addr, op->val);
			break;
		case B43_AC_OP_PHY_MOD:
			b43_phy_maskset(dev, op->addr, ~op->mask, op->val & op->mask);
			break;
		case B43_AC_OP_PHY_AND:
			b43_phy_mask(dev, op->addr, op->val);
			break;
		case B43_AC_OP_PHY_OR:
			b43_phy_set(dev, op->addr, op->val);
			break;
		case B43_AC_OP_RAD_W:
			b43_radio_write(dev, op->addr, op->val);
			break;
		case B43_AC_OP_RAD_MOD:
			b43_radio_maskset(dev, op->addr, ~op->mask,
					  op->val & op->mask);
			break;
		case B43_AC_OP_RAD_OR:
			b43_radio_set(dev, op->addr, op->val);
			break;
		case B43_AC_OP_TBL_W:
			b43_phy_ac_table_write(dev, op->addr, op->mask,
					       &b43_ac_replay_tbls[op->val]);
			break;
		case B43_AC_OP_DELAY:
			b43_phy_ac_delay(op->val);
			break;
		case B43_AC_OP_PMU:
			pmu = &b43_ac_replay_pmus[op->val];
			bcma_chipco_regctl_maskset(cc, op->addr, ~pmu->mask,
						   pmu->val & pmu->mask);
			break;
		case B43_AC_OP_SHM_W:
			b43_shm_write16(dev, B43_SHM_SHARED, op->addr, op->val);
			break;
		case B43_AC_OP_CC:
			pmu = &b43_ac_replay_pmus[op->val];
			bcma_cc_maskset32(cc, op->addr, ~pmu->mask, pmu->val);
			break;
		case B43_AC_OP_FGC:
			b43_phy_force_clock(dev, op->val);
			break;
		}
	}
}

static int b43_phy_ac_op_init(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;

	b43info(dev->wl, "AC-PHY rev %u, radio 0x%04x rev %u: init gravado do wl (%u ops, canal %d)\n",
		phy->rev, phy->radio_ver, phy->radio_rev, b43_ac_replay_nops,
		B43_AC_REPLAY_CHANNEL);
	if (phy->rev != 1 || phy->radio_ver != 0x2069 || phy->radio_rev != 4) {
		b43err(dev->wl, "init gravado e do 4360 B1 (AC rev 1, 2069 rev 4)\n");
		return -EOPNOTSUPP;
	}
	if (b43_current_band(dev->wl) != NL80211_BAND_5GHZ) {
		b43err(dev->wl, "init gravado e do radio de 5 GHz\n");
		return -EOPNOTSUPP;
	}
	if (ac_replay_ckpt > 0)
		mdelay(1500);
	b43_phy_ac_pmu_dump(dev, "antes");
	/* regcontrol 0: o wl no ar tem 0x00200004; depois do wl down (e no
	 * b43) fica 0x00200006.  O bit 1 nao aparece no rastro do PHY. */
	bcma_chipco_regctl_maskset(&dev->dev->bdev->bus->drv_cc, 0, ~0x2, 0);
	b43_phy_ac_replay(dev);
	b43_phy_ac_pmu_dump(dev, "depois");
	if (dev->dev->bus_type == B43_BUS_BCMA) {
		/* o wl termina o init do core d11 de 5 GHz com ioctrl 0x155:
		 * 0x145 do b43 + 0x10 (SICF_MPCLKE, clock do PHY pelo MAC) */
		u32 ioc = bcma_aread32(dev->dev->bdev, BCMA_IOCTL);

		bcma_awrite32(dev->dev->bdev, BCMA_IOCTL, ioc | 0x10);
		b43info(dev->wl, "AC-PHY: ioctrl %08x -> %08x\n", ioc,
			bcma_aread32(dev->dev->bdev, BCMA_IOCTL));
	}
	b43info(dev->wl, "replay: fim\n");
	dev->phy.ac->channel = B43_AC_REPLAY_CHANNEL;
	return 0;
}

static void b43_phy_ac_op_software_rfkill(struct b43_wldev *dev, bool blocked)
{
}

static void b43_phy_ac_op_switch_analog(struct b43_wldev *dev, bool on)
{
}

/* So o bloco da tabela de canais (radio 2069 + BW do PHY), que e o que o wl
 * escreve ao sintonizar; o resto da troca de canal (ganhos por banda,
 * calibracao) ainda nao. */
static int b43_phy_ac_op_switch_channel(struct b43_wldev *dev,
					unsigned int new_channel)
{
	const struct b43_radio_2069_channeltab_e *e = NULL;
	unsigned int i;

	if (new_channel == dev->phy.ac->channel)
		return 0;
	for (i = 0; i < b43_radio_2069_r4_chantab_size; i++) {
		if (b43_radio_2069_r4_chantab[i].channel == new_channel) {
			e = &b43_radio_2069_r4_chantab[i];
			break;
		}
	}
	if (!e)
		return -ESRCH;
	for (i = 0; i < B43_RADIO_2069_CHAN_REGS; i++)
		b43_radio_write(dev, b43_radio_2069_chan_regs[i], e->radio[i]);
	for (i = 0; i < ARRAY_SIZE(e->bw); i++)
		b43_phy_write(dev, B43_PHY_AC_BW1A + i, e->bw[i]);
	dev->phy.ac->channel = new_channel;
	b43dbg(dev->wl, "AC-PHY: canal %u (%u MHz) pela tabela do 2069\n",
	       new_channel, e->freq);
	return 0;
}

static unsigned int b43_phy_ac_op_get_default_chan(struct b43_wldev *dev)
{
	if (b43_current_band(dev->wl) == NL80211_BAND_2GHZ)
		return 11;
	return 36;
}

static enum b43_txpwr_result
b43_phy_ac_op_recalc_txpower(struct b43_wldev *dev, bool ignore_tssi)
{
	return B43_TXPWR_RES_DONE;
}

static void b43_phy_ac_op_adjust_txpower(struct b43_wldev *dev)
{
}

/**************************************************
 * PHY ops struct
 **************************************************/

const struct b43_phy_operations b43_phyops_ac = {
	.allocate		= b43_phy_ac_op_allocate,
	.free			= b43_phy_ac_op_free,
	.prepare_structs	= b43_phy_ac_op_prepare_structs,
	.init			= b43_phy_ac_op_init,
	.phy_maskset		= b43_phy_ac_op_maskset,
	.radio_read		= b43_phy_ac_op_radio_read,
	.radio_write		= b43_phy_ac_op_radio_write,
	.software_rfkill	= b43_phy_ac_op_software_rfkill,
	.switch_analog		= b43_phy_ac_op_switch_analog,
	.switch_channel		= b43_phy_ac_op_switch_channel,
	.get_default_chan	= b43_phy_ac_op_get_default_chan,
	.recalc_txpower		= b43_phy_ac_op_recalc_txpower,
	.adjust_txpower		= b43_phy_ac_op_adjust_txpower,
};
