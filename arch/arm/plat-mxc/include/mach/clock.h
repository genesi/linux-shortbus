/*
 * Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __ASM_ARCH_MXC_CLOCK_H__
#define __ASM_ARCH_MXC_CLOCK_H__

#ifndef __ASSEMBLY__
#include <linux/list.h>

struct module;

#ifdef CONFIG_USE_COMMON_STRUCT_CLK
#else
struct clk {
	int id;
	/* Source clock this clk depends on */
	struct clk *parent;
	/* Secondary clock to enable/disable with this clock */
	struct clk *secondary;
	/* Reference count of clock enable/disable */
	__s8 usecount;
	/* Register bit position for clock's enable/disable control. */
	u8 enable_shift;
	/* Register address for clock's enable/disable control. */
	void __iomem *enable_reg;
	u32 flags;
	/* get the current clock rate (always a fresh value) */
	unsigned long (*get_rate) (struct clk *);
	/* Function ptr to set the clock to a new rate. The rate must match a
	   supported rate returned from round_rate. Leave blank if clock is not
	   programmable */
	int (*set_rate) (struct clk *, unsigned long);
	/* Function ptr to round the requested clock rate to the nearest
	   supported rate that is less than or equal to the requested rate. */
	unsigned long (*round_rate) (struct clk *, unsigned long);
	/* Function ptr to enable the clock. Leave blank if clock can not
	   be gated. */
	int (*enable) (struct clk *);
	/* Function ptr to disable the clock. Leave blank if clock can not
	   be gated. */
	void (*disable) (struct clk *);
	/* Function ptr to set the parent clock of the clock. */
	int (*set_parent) (struct clk *, struct clk *);
};
#endif

int clk_register(struct clk *clk);
void clk_unregister(struct clk *clk);

unsigned long mxc_decode_pll(unsigned int pll, u32 f_ref);

extern spinlock_t imx_ccm_lock;

#define DEFINE_CLK_DIVIDER(name, _parent, _reg, _shift, _width) \
	struct clk_divider name = { \
		.clk = INIT_CLK(name.clk, clk_divider_ops), \
		.parent = (_parent), \
		.reg = (_reg), \
		.shift = (_shift), \
		.width = (_width), \
		.lock = &imx_ccm_lock, \
		.flags = CLK_DIVIDER_RATE_PROPAGATES, \
	}

#define DEFINE_CLK_MUX(name, _reg, _shift, _width, _clks) \
	struct clk_mux name = { \
		.clk = INIT_CLK(name.clk, clk_mux_ops), \
		.reg = (_reg), \
		.shift = (_shift), \
		.width = (_width), \
		.clks = (_clks), \
		.num_clks = ARRAY_SIZE(_clks), \
		.lock = &imx_ccm_lock, \
	}

/**
 * clock group
 *
 * @clk		clock source
 * @num_clks	number of parent clocks to enable
 * @clks	array of parents to enable/disable
 *
 * This clock is a groups of clocks useful for specifying clocks for
 * drivers which consist of multiple clocks. it enables/disables
 * all clocks in @clks, clk_get_rate/clk_set_rate are passed through
 * to the first member of @clks.
 */
struct clk_group {
	struct clk	clk;
	unsigned char	num_clks;
	struct clk	**clks;
};

extern struct clk_ops clk_group_ops;

#define DEFINE_CLK_GROUP(name, _clks) \
	struct clk_group name = { \
		.clk = INIT_CLK(name.clk, clk_group_ops), \
		.clks = (_clks), \
		.num_clks = ARRAY_SIZE(_clks), \
	}

/**
 * pll v2
 *
 * @clk		clock source
 * @parent	the parent clock
 * @base	base address of pll registers
 *
 * PLL clock version two, found on i.MX50/51/53
 */
struct clk_pllv2 {
	struct clk	clk;
	struct clk	*parent;
	void __iomem	*base;
};

extern struct clk_ops clk_pllv2_ops;

#define DEFINE_CLK_PLLV2(name, _parent, _base) \
	struct clk_pllv2 name = { \
		.clk = INIT_CLK(name.clk, clk_pllv2_ops), \
		.parent = (_parent), \
		.base = (_base), \
	}

#endif /* __ASSEMBLY__ */
#endif /* __ASM_ARCH_MXC_CLOCK_H__ */
