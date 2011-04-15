/*
 * Copyright (C) 2011 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/clkdev.h>

#include <asm/div64.h>

#include <mach/hardware.h>
#include <mach/common.h>
#include <mach/clock.h>

#include "crm_regs.h"

static void imx5_ccgr_enable(struct clk_gate *gate)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&imx_ccm_lock, flags);

	val = readl(gate->reg);
	val |= 3 << (gate->shift * 2);
	writel(val, gate->reg);

	spin_unlock_irqrestore(&imx_ccm_lock, flags);
}

static void imx5_ccgr_disable(struct clk_gate *gate)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&imx_ccm_lock, flags);

	val = readl(gate->reg);
	val &= ~(3 << (gate->shift * 2));
	writel(val, gate->reg);

	spin_unlock_irqrestore(&imx_ccm_lock, flags);
}

/* enable clock in run mode and disable in WAIT/STOP modes */
static void __maybe_unused imx5_ccgr_enable_runmode(struct clk_gate *gate)
{
	u32 val;

	val = readl(gate->reg);
	val &= ~(3 << (gate->shift * 2));
	val |= ~(1 << (gate->shift * 2));
	writel(val, gate->reg);
}

#define DEFINE_CLK_GATE(name, _parent, _reg, _shift) \
	struct clk_gate name = { \
		.clk = INIT_CLK(name.clk, clk_gate_ops), \
		.parent = (_parent), \
		.reg = (_reg), \
		.shift = (_shift), \
		.enable = imx5_ccgr_enable, \
		.disable = imx5_ccgr_disable, \
	}

static DEFINE_CLK_FIXED(ckil, 0);
static DEFINE_CLK_FIXED(osc, 0);
static DEFINE_CLK_FIXED(ckih1, 0);
static DEFINE_CLK_FIXED(ckih2, 0);

static DEFINE_CLK_PLLV2(pll1_sw, &osc.clk, MX51_DPLL1_BASE);
static DEFINE_CLK_PLLV2(pll2_sw, &osc.clk, MX51_DPLL2_BASE);
static DEFINE_CLK_PLLV2(pll3_sw, &osc.clk, MX51_DPLL3_BASE);
static DEFINE_CLK_PLLV2(pll4_sw, &osc.clk, MX53_DPLL4_BASE);

static DEFINE_CLK_FIXED(dummy, 0);

/* Low-power Audio Playback Mode clock */
static struct clk *lp_apm_sel[] = {
	&osc.clk,
	NULL,
};
static DEFINE_CLK_MUX(lp_apm, MXC_CCM_CCSR, 9, 1, lp_apm_sel);

/* This is used multiple times */
static struct clk *standard_pll_sel_clks[] = {
	&pll1_sw.clk,
	&pll2_sw.clk,
	&pll3_sw.clk,
	&lp_apm.clk,
};

static struct clk *periph_apm_sel[] = {
	&pll1_sw.clk,
	&pll3_sw.clk,
	&lp_apm.clk,
	NULL,
};
static DEFINE_CLK_MUX(periph_apm, MXC_CCM_CBCMR, 12, 2, periph_apm_sel);

static struct clk *main_bus_sel[] = {
	&pll2_sw.clk,
	&periph_apm.clk,
};
static DEFINE_CLK_MUX(main_bus, MXC_CCM_CBCDR, 25, 1, main_bus_sel);

static DEFINE_CLK_DIVIDER(ahb_root, &main_bus.clk, MXC_CCM_CBCDR, 10, 3);
static DEFINE_CLK_DIVIDER(ipg, &ahb_root.clk, MXC_CCM_CBCDR, 8, 2);

static DEFINE_CLK_DIVIDER(axi_a, &main_bus.clk, MXC_CCM_CBCDR, 16, 3);
static DEFINE_CLK_DIVIDER(axi_b, &main_bus.clk, MXC_CCM_CBCDR, 19, 3);

static DEFINE_CLK_MUX(uart_sel, MXC_CCM_CSCMR1, 24, 2, standard_pll_sel_clks);
static DEFINE_CLK_DIVIDER(uart_pred, &uart_sel.clk, MXC_CCM_CSCDR1, 3, 3);
static DEFINE_CLK_DIVIDER(uart_root, &uart_pred.clk, MXC_CCM_CSCDR1, 0, 3);

static DEFINE_CLK_MUX(esdhc1_sel, MXC_CCM_CSCMR1, 20, 2, standard_pll_sel_clks);
static DEFINE_CLK_DIVIDER(esdhc1_pred, &esdhc1_sel.clk, MXC_CCM_CSCDR1, 16, 3);
static DEFINE_CLK_DIVIDER(esdhc1_podf, &esdhc1_pred.clk, MXC_CCM_CSCDR1, 11, 3);

/* This is routed to esdhc3 in the i.MX53 datasheet */
static DEFINE_CLK_MUX(esdhc2_sel, MXC_CCM_CSCMR1, 16, 2, standard_pll_sel_clks);
static DEFINE_CLK_DIVIDER(esdhc2_pred, &esdhc2_sel.clk, MXC_CCM_CSCDR1, 22, 3);
static DEFINE_CLK_DIVIDER(esdhc2_podf, &esdhc2_pred.clk, MXC_CCM_CSCDR1, 19, 3);

static struct clk *esdhc3_sel_clks[] = {
	&esdhc1_podf.clk,
	&esdhc2_podf.clk,
};
static DEFINE_CLK_MUX(esdhc3_sel, MXC_CCM_CSCMR1, 19, 1, esdhc3_sel_clks);

static struct clk *esdhc4_sel_clks[] = {
	&esdhc1_podf.clk,
	&esdhc2_podf.clk,
};
static DEFINE_CLK_MUX(esdhc4_sel, MXC_CCM_CSCMR1, 18, 1, esdhc4_sel_clks);

static struct clk *emi_slow_sel_clks[] = {
	&main_bus.clk,
	&ahb_root.clk,
};
static DEFINE_CLK_MUX(emi_sel, MXC_CCM_CBCDR, 26, 1, emi_slow_sel_clks);
static DEFINE_CLK_DIVIDER(emi_slow_podf, &emi_sel.clk, MXC_CCM_CBCDR, 22, 3);
static DEFINE_CLK_DIVIDER(nfc_podf, &emi_slow_podf.clk, MXC_CCM_CBCDR, 13, 3);

static DEFINE_CLK_MUX(ecspi_sel, MXC_CCM_CSCMR1, 4, 2, standard_pll_sel_clks);
static DEFINE_CLK_DIVIDER(ecspi_pred, &ecspi_sel.clk, MXC_CCM_CSCDR2, 25, 3);
static DEFINE_CLK_DIVIDER(ecspi_podf, &ecspi_pred.clk, MXC_CCM_CSCDR2, 19, 6);

static DEFINE_CLK_MUX(usboh3_sel, MXC_CCM_CSCMR1, 22, 2, standard_pll_sel_clks);
static DEFINE_CLK_DIVIDER(usboh3_pred, &usboh3_sel.clk, MXC_CCM_CSCDR1, 8, 3);
static DEFINE_CLK_DIVIDER(usboh3_podf, &usboh3_pred.clk, MXC_CCM_CSCDR1, 6, 2);

static DEFINE_CLK_DIVIDER(usb_phy_pred, &pll3_sw.clk, MXC_CCM_CDCDR, 3, 3);
static DEFINE_CLK_DIVIDER(usb_phy_podf, &usb_phy_pred.clk, MXC_CCM_CDCDR, 0, 3);
static struct clk *usb_phy_sel_clks[] = {
	&osc.clk,
	&usb_phy_podf.clk,
};
static DEFINE_CLK_MUX(usb_phy_sel, MXC_CCM_CSCMR1, 26, 1, usb_phy_sel_clks);

static DEFINE_CLK_DIVIDER(cpu_podf, &pll1_sw.clk, MXC_CCM_CACRR, 0, 3);

static DEFINE_CLK_DIVIDER(di_pred, &pll3_sw.clk, MXC_CCM_CDCDR, 6, 3);

/* i.MX53 only */
static DEFINE_CLK_DIVIDER(di_pll4_podf, &pll4_sw.clk, MXC_CCM_CDCDR, 16, 3);

static struct clk *ipu_di0_sel_clks[] = {
	&di_pred.clk,
	&osc.clk,
	&ckih1.clk,
	NULL, /* i.MX51: &tve_di.clk i.MX53: di_pll4_podf */
	NULL, /* ipp di0 (iomux) */
	NULL, /* ldp di0 (i.MX53) */
	NULL, /* unused */
	NULL, /* unused */
};
static DEFINE_CLK_MUX(ipu_di0_sel, MXC_CCM_CSCMR2, 26, 3, ipu_di0_sel_clks);

static DEFINE_CLK_FIXED(tve_di, 65000000); /* FIXME */

static struct clk *ipu_di1_sel_clks[] = {
	&di_pred.clk,
	&osc.clk,
	&ckih1.clk,
	&tve_di.clk,
	NULL, /* ipp di1 (iomux) */
	NULL, /* ldp di1 (i.MX53) */
	NULL, /* unused */
	NULL, /* unused */
};
static DEFINE_CLK_MUX(ipu_di1_sel, MXC_CCM_CSCMR2, 29, 3, ipu_di1_sel_clks);

static struct clk *tve_ext_sel_clks[] = {
	&osc.clk,
	&ckih1.clk,
};
static DEFINE_CLK_MUX(tve_ext_sel, MXC_CCM_CSCMR1, 6, 1, tve_ext_sel_clks);

static DEFINE_CLK_DIVIDER(tve_pred, &pll3_sw.clk, MXC_CCM_CDCDR, 28, 3);

static struct clk *tve_sel_clks[] = {
	&tve_pred.clk,
	&tve_ext_sel.clk,
};
static DEFINE_CLK_MUX(tve_sel, MXC_CCM_CSCMR1, 7, 1, tve_sel_clks);

static DEFINE_CLK_GATE(iim_gate, &ipg.clk, MXC_CCM_CCGR0, 15);

static DEFINE_CLK_GATE(uart1_ipg_gate, &ipg.clk, MXC_CCM_CCGR1, 3);
static DEFINE_CLK_GATE(uart1_per_gate, &uart_root.clk, MXC_CCM_CCGR1, 4);
static DEFINE_CLK_GATE(uart2_ipg_gate, &ipg.clk, MXC_CCM_CCGR1, 5);
static DEFINE_CLK_GATE(uart2_per_gate, &uart_root.clk, MXC_CCM_CCGR1, 6);
static DEFINE_CLK_GATE(uart3_ipg_gate, &ipg.clk, MXC_CCM_CCGR1, 7);
static DEFINE_CLK_GATE(uart3_per_gate, &uart_root.clk, MXC_CCM_CCGR1, 8);
static DEFINE_CLK_GATE(i2c1_gate, &ipg.clk, MXC_CCM_CCGR1, 9);
static DEFINE_CLK_GATE(i2c2_gate, &ipg.clk, MXC_CCM_CCGR1, 10);
static DEFINE_CLK_GATE(hsi2c_gate, &ipg.clk, MXC_CCM_CCGR1, 11);

static DEFINE_CLK_GATE(mx51_usb_phy_gate, &usb_phy_sel.clk, MXC_CCM_CCGR2, 0);
static DEFINE_CLK_GATE(gpt_ipg_gate, &ipg.clk, MXC_CCM_CCGR2, 10);
static DEFINE_CLK_GATE(pwm1_ipg_gate, &ipg.clk, MXC_CCM_CCGR2, 5);
static DEFINE_CLK_GATE(pwm1_hf_gate, &ipg.clk, MXC_CCM_CCGR2, 6);
static DEFINE_CLK_GATE(pwm2_ipg_gate, &ipg.clk, MXC_CCM_CCGR2, 7);
static DEFINE_CLK_GATE(pwm2_hf_gate, &ipg.clk, MXC_CCM_CCGR2, 8);
static DEFINE_CLK_GATE(gpt_gate, &ipg.clk, MXC_CCM_CCGR2, 9);
static DEFINE_CLK_GATE(fec_gate, &ipg.clk, MXC_CCM_CCGR2, 12);
static DEFINE_CLK_GATE(usboh3_ahb_gate, &ipg.clk, MXC_CCM_CCGR2, 13);
static DEFINE_CLK_GATE(usboh3_gate, &usboh3_podf.clk, MXC_CCM_CCGR2, 14);
static DEFINE_CLK_GATE(tve_gate, &tve_sel.clk, MXC_CCM_CCGR2, 15);

static DEFINE_CLK_GATE(esdhc1_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 0);
static DEFINE_CLK_GATE(esdhc1_per_gate, &esdhc1_podf.clk, MXC_CCM_CCGR3, 1);
static DEFINE_CLK_GATE(esdhc2_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 2);
static DEFINE_CLK_GATE(esdhc2_per_gate, &esdhc2_podf.clk, MXC_CCM_CCGR3, 3);
static DEFINE_CLK_GATE(esdhc3_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 4);
static DEFINE_CLK_GATE(esdhc3_per_gate, &esdhc3_sel.clk, MXC_CCM_CCGR3, 5);
static DEFINE_CLK_GATE(esdhc4_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 6);
static DEFINE_CLK_GATE(esdhc4_per_gate, &esdhc4_sel.clk, MXC_CCM_CCGR3, 7);
static DEFINE_CLK_GATE(ssi1_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 8);
static DEFINE_CLK_GATE(ssi2_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 9);
static DEFINE_CLK_GATE(ssi3_ipg_gate, &ipg.clk, MXC_CCM_CCGR3, 10);

static DEFINE_CLK_GATE(mx51_mipi_hsc1_gate, &ipg.clk, MXC_CCM_CCGR4, 3);
static DEFINE_CLK_GATE(mx51_mipi_hsc2_gate, &ipg.clk, MXC_CCM_CCGR4, 4);
static DEFINE_CLK_GATE(mx51_mipi_esc_gate, &ipg.clk, MXC_CCM_CCGR4, 5);
static DEFINE_CLK_GATE(mx51_mipi_hsp_gate, &ipg.clk, MXC_CCM_CCGR4, 6);

static DEFINE_CLK_GATE(mx53_usb_phy1_gate, &usb_phy_sel.clk, MXC_CCM_CCGR4, 5);
static DEFINE_CLK_GATE(mx53_usb_phy2_gate, &usb_phy_sel.clk, MXC_CCM_CCGR4, 6);
static DEFINE_CLK_GATE(ecspi1_ipg_gate, &ipg.clk, MXC_CCM_CCGR4, 9);
static DEFINE_CLK_GATE(ecspi1_per_gate, &ecspi_podf.clk, MXC_CCM_CCGR4, 10);
static DEFINE_CLK_GATE(ecspi2_ipg_gate, &ipg.clk, MXC_CCM_CCGR4, 11);
static DEFINE_CLK_GATE(ecspi2_per_gate, &ecspi_podf.clk, MXC_CCM_CCGR4, 12);
static DEFINE_CLK_GATE(cspi_ipg_gate, &ipg.clk, MXC_CCM_CCGR4, 13);
static DEFINE_CLK_GATE(sdma_gate, &ipg.clk, MXC_CCM_CCGR4, 15);

static DEFINE_CLK_GATE(emi_fast_gate, &dummy.clk, MXC_CCM_CCGR5, 7);
static DEFINE_CLK_GATE(emi_slow_gate, &emi_slow_podf.clk, MXC_CCM_CCGR5, 8);

static struct clk *ipu_sel_clks[] = {
	&axi_a.clk,
	&axi_b.clk,
	&emi_slow_gate.clk,
	&ahb_root.clk,
};
static DEFINE_CLK_MUX(ipu_sel, MXC_CCM_CBCMR, 6, 2, ipu_sel_clks);

static DEFINE_CLK_GATE(ipu_gate, &ipu_sel.clk, MXC_CCM_CCGR5, 5);
static DEFINE_CLK_GATE(nfc_gate, &nfc_podf.clk, MXC_CCM_CCGR5, 10);

static DEFINE_CLK_GATE(ipu_di0_gate, &ipu_di0_sel.clk, MXC_CCM_CCGR6, 5);
static DEFINE_CLK_GATE(ipu_di1_gate, &ipu_di1_sel.clk, MXC_CCM_CCGR6, 6);

static DEFINE_CLK_GATE(vpu_gate, &nfc_podf.clk, MXC_CCM_CCGR5, 3);
static DEFINE_CLK_GATE(vpu_serial_gate, &nfc_podf.clk, MXC_CCM_CCGR5, 4);

static struct clk *vpu_sel_clks[] = {
	&axi_a.clk,
	&axi_b.clk,
	&emi_slow_gate.clk,
	&ahb_root.clk,
};
static DEFINE_CLK_MUX(vpu_sel, MXC_CCM_CBCMR, 14, 2, vpu_sel_clks);

static struct clk *vpu_group[] = {
	&vpu_gate.clk,
	&vpu_serial_gate.clk
};
static DEFINE_CLK_GROUP(vpu, vpu_group);

static struct clk *uart1_group[] = {
	&uart1_per_gate.clk,
	&uart1_ipg_gate.clk
};
static DEFINE_CLK_GROUP(uart1, uart1_group);

static struct clk *uart2_group[] = {
	&uart2_per_gate.clk,
	&uart2_ipg_gate.clk
};
static DEFINE_CLK_GROUP(uart2, uart2_group);

static struct clk *uart3_group[] = {
	&uart3_per_gate.clk,
	&uart3_ipg_gate.clk
};
static DEFINE_CLK_GROUP(uart3, uart3_group);

static struct clk *gpt_group[] = {
	&gpt_gate.clk,
	&gpt_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(gpt, gpt_group);

static struct clk *esdhc1_group[] = {
	&esdhc1_per_gate.clk,
	&esdhc1_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(esdhc1, esdhc1_group);

static struct clk *esdhc2_group[] = {
	&esdhc2_per_gate.clk,
	&esdhc2_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(esdhc2, esdhc2_group);

static struct clk *esdhc3_group[] = {
	&esdhc3_per_gate.clk,
	&esdhc3_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(esdhc3, esdhc3_group);

static struct clk *esdhc4_group[] = {
	&esdhc4_per_gate.clk,
	&esdhc4_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(esdhc4, esdhc4_group);

static struct clk *ecspi1_group[] = {
	&ecspi1_per_gate.clk,
	&ecspi1_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(ecspi1, ecspi1_group);

static struct clk *ecspi2_group[] = {
	&ecspi2_per_gate.clk,
	&ecspi2_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(ecspi2, ecspi2_group);

static struct clk *cspi_group[] = {
	&ecspi1_per_gate.clk,
	&cspi_ipg_gate.clk,
};
static DEFINE_CLK_GROUP(cspi, cspi_group);

static struct clk *mx51_mipi_group[] = {
	&mx51_mipi_hsp_gate.clk,
	&mx51_mipi_hsc1_gate.clk,
	&mx51_mipi_hsc2_gate.clk,
	&mx51_mipi_esc_gate.clk,
};
static DEFINE_CLK_GROUP(mx51_mipi, mx51_mipi_group);

static struct clk *ipu_group_clks[] = {
	&ipu_gate.clk,
	&emi_fast_gate.clk,
};
static DEFINE_CLK_GROUP(ipu, ipu_group_clks);

#define _REGISTER_CLOCK(d, n, c) \
       { \
		.dev_id = d, \
		.con_id = n, \
		.clk = c,   \
       },

static struct clk_lookup mx5_lookups[] = {
	_REGISTER_CLOCK("imx-uart.0", NULL, &uart1.clk)
	_REGISTER_CLOCK("imx-uart.1", NULL, &uart2.clk)
	_REGISTER_CLOCK("imx-uart.2", NULL, &uart3.clk)
	_REGISTER_CLOCK("fec.0", NULL, &fec_gate.clk)
	_REGISTER_CLOCK("mxc_pwm.0", "pwm", &pwm1_ipg_gate.clk)
	_REGISTER_CLOCK("mxc_pwm.1", "pwm", &pwm2_ipg_gate.clk)
	_REGISTER_CLOCK("imx-i2c.0", NULL, &i2c1_gate.clk)
	_REGISTER_CLOCK("imx-i2c.1", NULL, &i2c2_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.0", "usb", &usboh3_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.0", "usb_ahb", &usboh3_ahb_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.0", "usb_phy1", &mx53_usb_phy1_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.1", "usb", &usboh3_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.1", "usb_ahb", &usboh3_ahb_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.2", "usb", &usboh3_gate.clk)
	_REGISTER_CLOCK("mxc-ehci.2", "usb_ahb", &usboh3_ahb_gate.clk)
	_REGISTER_CLOCK("fsl-usb2-udc", "usb", &usboh3_gate.clk)
	_REGISTER_CLOCK("fsl-usb2-udc", "usb_ahb", &usboh3_ahb_gate.clk)
	_REGISTER_CLOCK("mxc_nand", NULL, &nfc_gate.clk)
	_REGISTER_CLOCK("imx-ssi.0", NULL, &ssi1_ipg_gate.clk)
	_REGISTER_CLOCK("imx-ssi.1", NULL, &ssi1_ipg_gate.clk)
	_REGISTER_CLOCK("imx-ssi.2", NULL, &ssi1_ipg_gate.clk)
	_REGISTER_CLOCK("imx-sdma", NULL, &sdma_gate.clk)
	_REGISTER_CLOCK("imx51-ecspi.0", NULL, &ecspi1.clk)
	_REGISTER_CLOCK("imx51-ecspi.1", NULL, &ecspi2.clk)
	_REGISTER_CLOCK("imx51-cspi.0", NULL, &cspi.clk)
	_REGISTER_CLOCK(NULL, "cpu", &cpu_podf.clk)
	_REGISTER_CLOCK(NULL, "iim", &iim_gate.clk)
	_REGISTER_CLOCK("sdhci-esdhc-imx.0", NULL, &esdhc1.clk)
	_REGISTER_CLOCK("sdhci-esdhc-imx.1", NULL, &esdhc2.clk)
	_REGISTER_CLOCK("sdhci-esdhc-imx.2", NULL, &esdhc3.clk)
	_REGISTER_CLOCK("sdhci-esdhc-imx.3", NULL, &esdhc4.clk)
	_REGISTER_CLOCK("imx-ipuv3", NULL, &ipu.clk)
	_REGISTER_CLOCK("imx-ipuv3", "di0", &ipu_di0_gate.clk)
	_REGISTER_CLOCK("imx-ipuv3", "di1", &ipu_di1_gate.clk)
	_REGISTER_CLOCK("imx2-wdt.0", NULL, &dummy.clk)
	_REGISTER_CLOCK("imx2-wdt.1", NULL, &dummy.clk)
	_REGISTER_CLOCK("imx-keypad", NULL, &dummy.clk)
	_REGISTER_CLOCK("imx-tve.0", NULL, &tve_gate.clk)
	_REGISTER_CLOCK("imx-tve.0", "di1", &ipu_di1_gate.clk)
};

static struct clk_lookup mx51_lookups[] = {
	_REGISTER_CLOCK("imx-i2c.2", NULL, &hsi2c_gate.clk)
	_REGISTER_CLOCK(NULL, "mipi_hsp", &mx51_mipi.clk)
	_REGISTER_CLOCK("imx51-vpu.0", NULL, &vpu.clk)
};

static struct clk_lookup mx53_lookups[] = {
	_REGISTER_CLOCK("imx53-vpu.0", NULL, &vpu.clk)
};

static void mx5_clocks_common_init(unsigned long rate_ckil, unsigned long rate_osc,
			unsigned long rate_ckih1, unsigned long rate_ckih2)
{
	ckil.rate = rate_ckil;
	osc.rate = rate_osc;
	ckih1.rate = rate_ckih1;
	ckih2.rate = rate_ckih2;

	clkdev_add_table(mx5_lookups, ARRAY_SIZE(mx5_lookups));

	/* Set SDHC parents to be PLL2 */
	clk_set_parent(&esdhc1_sel.clk, &pll2_sw.clk);
	clk_set_parent(&esdhc2_sel.clk, &pll2_sw.clk);
}

int __init mx51_clocks_init(unsigned long rate_ckil, unsigned long rate_osc,
			unsigned long rate_ckih1, unsigned long rate_ckih2)
{
	unsigned long r;

	mx5_clocks_common_init(rate_ckil, rate_osc, rate_ckih1, rate_ckih2);
	clkdev_add_table(mx51_lookups, ARRAY_SIZE(mx51_lookups));

	/* set SDHC root clock to 166.25MHZ*/
	clk_set_rate(&esdhc1_podf.clk, 166250000);
	clk_set_rate(&esdhc2_podf.clk, 166250000);

	/* System timer */
	mxc_timer_init(&gpt.clk, MX51_IO_ADDRESS(MX51_GPT1_BASE_ADDR),
		MX51_MXC_INT_GPT);

	clk_prepare(&iim_gate.clk);
	clk_enable(&iim_gate.clk);
	mx51_revision();
	clk_disable(&iim_gate.clk);
	mx51_display_revision();

	di_pred.flags &= ~CLK_DIVIDER_RATE_PROPAGATES; /* FIXME */

	r = clk_round_rate(&di_pred.clk, 216000000 / 2);
	clk_set_rate(&di_pred.clk, r);

	return 0;
}

int __init mx53_clocks_init(unsigned long rate_ckil, unsigned long rate_osc,
			unsigned long rate_ckih1, unsigned long rate_ckih2)
{
	unsigned long r;
	int ret;

	di_pll4_podf.flags &= ~CLK_DIVIDER_RATE_PROPAGATES; /* FIXME */

	/* Clock tree has i.MX51 layout. i.MX53 needs some fixups */
	pll1_sw.base = MX53_DPLL1_BASE;
	pll2_sw.base = MX53_DPLL2_BASE;
	pll3_sw.base = MX53_DPLL3_BASE;
	esdhc3_per_gate.parent = &esdhc2_podf.clk;
	esdhc2_per_gate.parent = &esdhc3_sel.clk;
	tve_gate.parent = &tve_pred.clk;
	tve_pred.parent = &tve_ext_sel.clk;
	tve_ext_sel_clks[0] = &pll4_sw.clk;
	tve_ext_sel_clks[1] = &ckih1.clk;
	ipu_di0_sel_clks[3] = &di_pll4_podf.clk;

	clk_set_parent(&ipu_di0_sel.clk, &di_pll4_podf.clk);

	clk_set_parent(&tve_ext_sel.clk, &pll4_sw.clk);
	clk_set_parent(&ipu_di1_sel.clk, &tve_di.clk);

	mx5_clocks_common_init(rate_ckil, rate_osc, rate_ckih1, rate_ckih2);
	clkdev_add_table(mx53_lookups, ARRAY_SIZE(mx53_lookups));

	ret = clk_set_rate(&pll4_sw.clk, 150000000);
	if (ret)
		printk("pll4 set failed: %d\n", ret);

	r = clk_round_rate(&di_pll4_podf.clk, 150000000);
	clk_set_rate(&di_pll4_podf.clk, r);

	/* set SDHC root clock to 200MHZ*/
	clk_set_rate(&esdhc1_podf.clk, 200000000);
	clk_set_rate(&esdhc2_podf.clk, 200000000);

	/* System timer */
	mxc_timer_init(&gpt.clk, MX53_IO_ADDRESS(MX53_GPT1_BASE_ADDR),
		MX53_INT_GPT);

	clk_prepare(&iim_gate.clk);
	clk_enable(&iim_gate.clk);
	mx53_revision();
	clk_disable(&iim_gate.clk);

	return 0;
}

