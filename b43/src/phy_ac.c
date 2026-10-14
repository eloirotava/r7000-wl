// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Broadcom B43 wireless driver
 * IEEE 802.11ac AC-PHY support
 *
 * Copyright (c) 2015 Rafał Miłecki <zajec5@gmail.com>
 */

#include "b43.h"
#include "phy_ac.h"

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

/* marco 1: so identifica PHY/radio; init real do PHY e do 2069 ainda nao */
static int b43_phy_ac_op_init(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;

	b43info(dev->wl, "AC-PHY rev %u, radio 0x%04x rev %u (init ainda vazio)\n",
		phy->rev, phy->radio_ver, phy->radio_rev);
	return 0;
}

static void b43_phy_ac_op_software_rfkill(struct b43_wldev *dev, bool blocked)
{
}

static void b43_phy_ac_op_switch_analog(struct b43_wldev *dev, bool on)
{
}

static int b43_phy_ac_op_switch_channel(struct b43_wldev *dev,
					unsigned int new_channel)
{
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
