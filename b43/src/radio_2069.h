/* SPDX-License-Identifier: GPL-2.0 */
#ifndef B43_RADIO_2069_H_
#define B43_RADIO_2069_H_

#include <linux/types.h>

/* registradores do radio escritos em cada troca de canal, na ordem de
 * b43_radio_2069_channeltab_e.radio (radio_2069.c) */
#define B43_RADIO_2069_CHAN_REGS	50

struct b43_radio_2069_channeltab_e {
	u16 channel;
	u16 freq;
	u16 radio[B43_RADIO_2069_CHAN_REGS];
	u16 bw[6];		/* PHY 0x371-0x376 (B43_PHY_AC_BW1A..BW6) */
};

extern const u16 b43_radio_2069_chan_regs[B43_RADIO_2069_CHAN_REGS];
extern const struct b43_radio_2069_channeltab_e b43_radio_2069_r4_chantab[];
extern const unsigned int b43_radio_2069_r4_chantab_size;

#endif /* B43_RADIO_2069_H_ */
