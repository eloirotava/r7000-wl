/* SPDX-License-Identifier: GPL-2.0 */
#ifndef B43_PHY_AC_REPLAY_H_
#define B43_PHY_AC_REPLAY_H_

#include <linux/types.h>

/* operacoes do init gravado do wl (phy_ac_replay.c, tools/gen-replay.py) */
enum {
	B43_AC_OP_PHY_W = 1,	/* addr = val */
	B43_AC_OP_PHY_MOD,	/* addr = (addr & ~mask) | (val & mask) */
	B43_AC_OP_PHY_AND,	/* addr &= val */
	B43_AC_OP_PHY_OR,	/* addr |= val */
	B43_AC_OP_RAD_W,
	B43_AC_OP_RAD_MOD,
	B43_AC_OP_RAD_OR,
	B43_AC_OP_TBL_W,	/* addr = id, mask = offset, val = indice em tbls */
	B43_AC_OP_DELAY,	/* val = us */
	B43_AC_OP_PMU,		/* addr = regcontrol, val = indice em pmus */
	B43_AC_OP_SHM_W,	/* SHM compartilhada, addr em bytes */
	B43_AC_OP_FGC,		/* val = forca o clock do PHY (1) ou solta (0) */
};

struct b43_ac_replay_op {
	u8 type;
	u16 addr;
	u16 mask;
	u16 val;
};

struct b43_ac_replay_tbl {
	u8 width;		/* 8, 16, 32 ou 48 */
	u16 len;		/* entradas */
	u32 start;		/* em b43_ac_replay_data; w48 usa 3 valores */
};

struct b43_ac_replay_pmu {
	u32 mask;
	u32 val;
};

/* canal em que o init gravado termina */
#define B43_AC_REPLAY_CHANNEL	36

extern const struct b43_ac_replay_op b43_ac_replay_ops[];
extern const unsigned int b43_ac_replay_nops;
extern const struct b43_ac_replay_tbl b43_ac_replay_tbls[];
extern const struct b43_ac_replay_pmu b43_ac_replay_pmus[];
extern const u32 b43_ac_replay_data[];

#endif /* B43_PHY_AC_REPLAY_H_ */
