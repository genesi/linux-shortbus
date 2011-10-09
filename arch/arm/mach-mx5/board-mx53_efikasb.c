/*
 * Copyright (C) 2011 Genesi USA, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/init.h>
#include <linux/clk.h>
#include <linux/fec.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mxcfb.h>
#include <linux/ipu.h>
#include <linux/pwm_backlight.h>
#include <linux/ahci_platform.h>
#include <linux/i2c.h>
#include <linux/i2c/mpr.h>
#include <linux/fsl_devices.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/imx-uart.h>
#include <mach/iomux-mx53.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_dvfs.h>
#include <asm/mach/flash.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include "crm_regs.h"
#include "devices-imx53.h"
#include "devices.h"
#include "usb.h"

/* MX53 Efika SB GPIO PIN configurations */
#define USBDR_OC	            IMX_GPIO_NR(4, 14) /* GPIO_4_14 */
#define USBDR_PWREN             IMX_GPIO_NR(4, 15) /* GPIO_4_15 */
#define USBH1_OC                IMX_GPIO_NR(3, 30) /* GPIO_3_30 */
#define USBH1_PWREN             IMX_GPIO_NR(3, 31) /* GPIO_3_31 */
#define CLKO                    IMX_GPIO_NR(1, 5)  /* GPIO_1_5 */
#define WIFISWITCH              IMX_GPIO_NR(1, 0)  /* GPIO_1_0 */
#define SD1_WP                  IMX_GPIO_NR(1, 9)  /* GPIO_1_9 */

#define PERIPH_RESET            IMX_GPIO_NR(5, 28) /* GPIO_5_28 */
#define PERIPH_PWR		        IMX_GPIO_NR(5, 29) /* GPIO_5_29 */

/* GPIO I2C */
#define GPIO_SDA                IMX_GPIO_NR(5, 27) /* GPIO_5_27 */
#define GPIO_SCL                IMX_GPIO_NR(5, 26) /* GPIO_5_26 */

static iomux_v3_cfg_t mx53_efikasb_pads[] = {
	/* USB */
	MX53_PAD_KEY_COL4__GPIO4_14,    /* overcurrent */
	MX53_PAD_KEY_ROW4__GPIO4_15,    /* pwr_en */
	MX53_PAD_EIM_D30__GPIO3_30,     /* overcurrent input */
	MX53_PAD_EIM_D31__GPIO3_31,     /* power enable */
	/* AUDIO */
	MX53_PAD_CSI0_DAT4__AUDMUX_AUD3_TXC,
	MX53_PAD_CSI0_DAT5__AUDMUX_AUD3_TXD,
	MX53_PAD_CSI0_DAT6__AUDMUX_AUD3_TXFS,
	MX53_PAD_CSI0_DAT7__AUDMUX_AUD3_RXD,
	/* PERIPHERAL */
	MX53_PAD_GPIO_5__CCM_CLKO,       /* CLKO master audio/camera clock */
	MX53_PAD_GPIO_0__GPIO1_0,        /* WIFI switch */
	MX53_PAD_CSI0_DAT10__GPIO5_28,   /* Periph Reset */
	MX53_PAD_CSI0_DAT11__GPIO5_29,   /* Periph Power Down */
	/* SD1 */
	MX53_PAD_SD1_CMD__ESDHC1_CMD,
	MX53_PAD_SD1_CLK__ESDHC1_CLK,
	MX53_PAD_SD1_DATA0__ESDHC1_DAT0,
	MX53_PAD_SD1_DATA1__ESDHC1_DAT1,
	MX53_PAD_SD1_DATA2__ESDHC1_DAT2,
	MX53_PAD_SD1_DATA3__ESDHC1_DAT3,
	MX53_PAD_GPIO_9__GPIO1_9,           /* SD1 write protect */
	/* SD2 */
	MX53_PAD_SD2_CMD__ESDHC2_CMD,
	MX53_PAD_SD2_CLK__ESDHC2_CLK,
	MX53_PAD_SD2_DATA0__ESDHC2_DAT0,
	MX53_PAD_SD2_DATA1__ESDHC2_DAT1,
	MX53_PAD_SD2_DATA2__ESDHC2_DAT2,
	MX53_PAD_SD2_DATA3__ESDHC2_DAT3,
	/* LVDS */
/*	MX53_PAD_LVDS0_TX3_P__LDB_LVDS0_TX3, */ /* Schematic says no */
	MX53_PAD_LVDS0_CLK_P__LDB_LVDS0_CLK,
	MX53_PAD_LVDS0_TX2_P__LDB_LVDS0_TX2,
	MX53_PAD_LVDS0_TX1_P__LDB_LVDS0_TX1,
	MX53_PAD_LVDS0_TX0_P__LDB_LVDS0_TX0,
	/* I2C1 */
/*	MX53_PAD_CSI0_DAT8__I2C1_SDA, */
/*	MX53_PAD_CSI0_DAT9__I2C1_SCL, */
	MX53_PAD_CSI0_DAT8__GPIO5_26,
	MX53_PAD_CSI0_DAT9__GPIO5_27,
	/* UART */
	MX53_PAD_PATA_DIOW__UART1_TXD_MUX,
	MX53_PAD_PATA_DMACK__UART1_RXD_MUX,
	MX53_PAD_PATA_DMARQ__UART2_TXD_MUX,
	MX53_PAD_PATA_BUFFER_EN__UART2_RXD_MUX,
	/* CAMERA */
	MX53_PAD_CSI0_DAT12__IPU_CSI0_D_12,
	MX53_PAD_CSI0_DAT13__IPU_CSI0_D_13,
	MX53_PAD_CSI0_DAT14__IPU_CSI0_D_14,
	MX53_PAD_CSI0_DAT15__IPU_CSI0_D_15,
	MX53_PAD_CSI0_DAT16__IPU_CSI0_D_16,
	MX53_PAD_CSI0_DAT17__IPU_CSI0_D_17,
	MX53_PAD_CSI0_DAT18__IPU_CSI0_D_18,
	MX53_PAD_CSI0_DAT19__IPU_CSI0_D_19,
	MX53_PAD_CSI0_VSYNC__IPU_CSI0_VSYNC,
	MX53_PAD_CSI0_MCLK__IPU_CSI0_HSYNC,
	MX53_PAD_CSI0_PIXCLK__IPU_CSI0_PIXCLK,
};

static iomux_v3_cfg_t mx53_efikasb_nand_pads[] = {
	MX53_PAD_NANDF_CLE__EMI_NANDF_CLE,
	MX53_PAD_NANDF_ALE__EMI_NANDF_ALE,
	MX53_PAD_NANDF_WP_B__EMI_NANDF_WP_B,
	MX53_PAD_NANDF_WE_B__EMI_NANDF_WE_B,
	MX53_PAD_NANDF_RE_B__EMI_NANDF_RE_B,
	MX53_PAD_NANDF_RB0__EMI_NANDF_RB_0,
	MX53_PAD_NANDF_CS0__EMI_NANDF_CS_0,
	MX53_PAD_NANDF_CS1__EMI_NANDF_CS_1,
	MX53_PAD_NANDF_CS2__EMI_NANDF_CS_2, /* -->  not connected */
	MX53_PAD_NANDF_CS3__EMI_NANDF_CS_3, /* -->  not connected */
	MX53_PAD_EIM_DA0__EMI_NAND_WEIM_DA_0,
	MX53_PAD_EIM_DA1__EMI_NAND_WEIM_DA_1,
	MX53_PAD_EIM_DA2__EMI_NAND_WEIM_DA_2,
	MX53_PAD_EIM_DA3__EMI_NAND_WEIM_DA_3,
	MX53_PAD_EIM_DA4__EMI_NAND_WEIM_DA_4,
	MX53_PAD_EIM_DA5__EMI_NAND_WEIM_DA_5,
	MX53_PAD_EIM_DA6__EMI_NAND_WEIM_DA_6,
	MX53_PAD_EIM_DA7__EMI_NAND_WEIM_DA_7,
};

static struct fb_videomode video_modes[] = {
	{
		/* WSVGA 1024x600 @ 60 Hz */
		"WSVGA", 60, 1024, 600, 22800,
		80, 40,
		20, 21,
		4, 4,
		FB_SYNC_OE_LOW_ACT,
		FB_VMODE_NONINTERLACED,
		0,},
};

static struct ipuv3_fb_platform_data efikasb_fb0_data = {
	.interface_pix_fmt = IPU_PIX_FMT_BGR24,
	.mode_str = "WSVGA",
	.modes = video_modes,
	.num_modes = ARRAY_SIZE(video_modes),
};

static struct ipuv3_fb_platform_data efikasb_fb1_data = {
	.interface_pix_fmt = IPU_PIX_FMT_BGR24,
	.mode_str = "WSVGA",
	.modes = video_modes,
	.num_modes = ARRAY_SIZE(video_modes),
};

static struct imx_ipuv3_platform_data ipu_data = {
	.rev = 3,
	.fb_head0_platform_data = &efikasb_fb0_data,
	.fb_head1_platform_data = &efikasb_fb1_data,
	.primary_di = MXC_PRI_DI0,
};

static struct fsl_mxc_ldb_platform_data ldb_data = {
	.ext_ref = 1,
};

static struct mxc_dvfs_platform_data efikasb_dvfs_core_data = {
	.reg_id = "DA9052_BUCK_CORE",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_offset = MXC_GPC_CNTR_OFFSET,
	.gpc_vcr_offset = MXC_GPC_VCR_OFFSET,
	.ccm_cdcr_offset = MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset = MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset = MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask = 0x1F800,
	.prediv_offset = 11,
	.prediv_val = 3,
	.div3ck_mask = 0xE0000000,
	.div3ck_offset = 29,
	.div3ck_val = 2,
	.emac_val = 0x08,
	.upthr_val = 25,
	.dnthr_val = 9,
	.pncthr_val = 33,
	.upcnt_val = 10,
	.dncnt_val = 10,
	.delay_time = 30,
};


static const struct esdhc_platform_data mx53_efikasb_sd1_data __initconst = {
	.always_present = true,
	.wp_gpio = SD1_WP,
};

static const struct esdhc_platform_data mx53_efikasb_sd2_data __initconst = {
	.always_present = true,
};

static struct fsl_mxc_camera_platform_data camera_data = {
	.mclk = 27000000,
	.csi = 0,
};

static struct mxc_audio_platform_data cs42l52_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.sysclk = 27000000,
};

static struct platform_device cs42l52_device = {
	.name = "imx-cs42l52",
};


static const struct imxi2c_platform_data mx53_efikasb_i2c_data __initconst = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
	.type = "cs42l52",
	.addr = 0x4a,
	 },
	{
	.type = "gc0308",
	.addr = 0x42,
	.platform_data = (void *)&camera_data,
	},
};

/* Temporary GPIO I2C solution */
static struct i2c_gpio_platform_data i2c_gpio_data = {
	.sda_pin	= GPIO_SDA,
	.scl_pin	= GPIO_SCL,
};

static struct platform_device i2c_gpio_device = {
	.name		= "i2c-gpio",
	.id			= 0,
};

static int nand_init(void)
{
	u32 i, reg;
	void __iomem *base;

#define M4IF_GENP_WEIM_MM_MASK          0x00000001
#define WEIM_GCR2_MUX16_BYP_GRANT_MASK  0x00001000
	
	base = ioremap(MX53_M4IF_BASE_ADDR, SZ_4K);
	reg = __raw_readl(base + 0xc);
	reg &= ~M4IF_GENP_WEIM_MM_MASK;
	__raw_writel(reg, base + 0xc);
	
	iounmap(base);
	
	base = ioremap(MX53_WEIM_BASE_ADDR, SZ_4K);
	for (i = 0x4; i < 0x94; i += 0x18) {
		reg = __raw_readl((u32)base + i);
		reg &= ~WEIM_GCR2_MUX16_BYP_GRANT_MASK;
		__raw_writel(reg, (u32)base + i);
	}
	iounmap(base);
	
	return 0;
}


/* NAND Flash Partitions */
#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition nand_flash_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 32 * 1024 * 1024},
	{
	 .name = "nand.kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = 128 * 1024 * 1024},
	{
	 .name = "nand.rootfs",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL},
};
#endif

static struct flash_platform_data efikasb_nand_data = {
#ifdef CONFIG_MTD_PARTITIONS
	.parts = nand_flash_partitions,
	.nr_parts = ARRAY_SIZE(nand_flash_partitions),
#endif
	.width = 1,
	.init = nand_init,
};

static struct resource mxc_nand_resources[] = {
	{
		.flags = IORESOURCE_MEM,
		.name  = "NFC_AXI_BASE",
		.start = MX53_NFC_AXI_BASE_ADDR	,
		.end   = MX53_NFC_AXI_BASE_ADDR	 + SZ_8K - 1,
	},
	{
		.flags = IORESOURCE_MEM,
		.name  = "NFC_IP_BASE",
		.start = MX53_NFC_BASE_ADDR + 0x00,
		.end   = MX53_NFC_BASE_ADDR + 0x34 - 1,
	},
	{
		.flags = IORESOURCE_IRQ,
		.start = MX53_INT_NFC,
		.end   = MX53_INT_NFC,
	},
};

struct platform_device mxc_nandv2_mtd_device = {
	.name = "mxc_nandv2_flash",
	.id = 0,
	.resource = mxc_nand_resources,
	.num_resources = ARRAY_SIZE(mxc_nand_resources),
};


static void mx53_efikasb_usbh1_vbus(bool on)
{
	if (on)
		gpio_set_value(USBH1_PWREN, 1);
	else
		gpio_set_value(USBH1_PWREN, 0);
}

static void mx53_efikasb_usbdr_vbus(bool on)
{
	if (on)
		gpio_set_value(USBDR_PWREN, 1);
	else
		gpio_set_value(USBDR_PWREN, 0);
}

static void mx53_efikasb_wlan_power(bool on)
{
	if (on)
		gpio_set_value(WIFISWITCH, 1);
	else
		gpio_set_value(WIFISWITCH, 0);
}

static void mx53_efikasb_periph_power(bool on)
{
	if (on)
		gpio_set_value(PERIPH_PWR, 1);
	else
		gpio_set_value(PERIPH_PWR, 0);
}

static void mx53_efikasb_do_periph_reset(void)
{
	gpio_set_value(PERIPH_RESET, 1);
	msleep(50);
	gpio_set_value(PERIPH_RESET, 0);
	msleep(1);
	gpio_set_value(PERIPH_RESET, 1);
	msleep(30);
}

static struct imx_ssi_platform_data efikasb_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN |
					IMX_SSI_NET | IMX_SSI_USE_I2S_SLAVE,
};


static struct mxc_gpu_platform_data gpu_data __initdata;
/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int left_mem = 0;
	int gpu_mem = SZ_32M;
	int fb_mem = SZ_16M;
	char *str;

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			left_mem = total_mem - gpu_mem - fb_mem;
			break;
		}
	}

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "mem=");
			if (str != NULL) {
				str += 4;
				left_mem = memparse(str, &str);
				if (left_mem == 0 || left_mem > total_mem)
					left_mem = total_mem - gpu_mem - fb_mem;
			}

			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpu_memory=");
			if (str != NULL) {
				str += 11;
				gpu_mem = memparse(str, &str);
			}

			break;
		}
	}

	if (mem_tag) {
		fb_mem = total_mem - left_mem - gpu_mem;
		if (fb_mem < 0) {
			gpu_mem = total_mem - left_mem;
			fb_mem = 0;
		}
		mem_tag->u.mem.size = left_mem;

		/* reserve memory for gpu */
		gpu_data.reserved_mem_base =
				mem_tag->u.mem.start + left_mem;
		gpu_data.reserved_mem_size = gpu_mem;

		/* reserve memory for fb */
		efikasb_fb0_data.res_base = gpu_data.reserved_mem_base
					+ gpu_data.reserved_mem_size;
		efikasb_fb0_data.res_size = fb_mem;
		efikasb_fb1_data.res_base = gpu_data.reserved_mem_base
					+ gpu_data.reserved_mem_size;
		efikasb_fb1_data.res_size = fb_mem;
	}
}


static void __init mx53_efikasb_io_init(void)
{

	imx_otg_base = MX53_IO_ADDRESS(MX53_OTG_BASE_ADDR);

	mxc_iomux_v3_setup_multiple_pads(mx53_efikasb_pads,
					ARRAY_SIZE(mx53_efikasb_pads));

	mxc_iomux_v3_setup_multiple_pads(mx53_efikasb_nand_pads,
					ARRAY_SIZE(mx53_efikasb_nand_pads));

	/* SD1 Write Protect */
	gpio_request(SD1_WP, "sd1-wp");
	gpio_direction_input(SD1_WP);
	gpio_free(SD1_WP);

	/* USB PWR enable */
	gpio_request(USBH1_PWREN, "usb-pwr1");
	gpio_direction_output(USBH1_PWREN, 0);

	gpio_request(USBDR_PWREN, "usb-pwr0");
	gpio_direction_output(USBDR_PWREN, 0);

	/* WIFI power */
	gpio_request(WIFISWITCH, "wifi-pwr");
	gpio_direction_output(WIFISWITCH, 0);

	/* Peripheral power */
	gpio_request(PERIPH_PWR, "periph-pwr");
	gpio_direction_output(PERIPH_PWR, 0);

	/* Peripheral reset */
	gpio_request(PERIPH_RESET, "periph-reset");
	gpio_direction_output(PERIPH_RESET, 0);

}

/* Sets up the MCLK for audio and camera */
static void efikasb_mclk_init(void)
{

	struct clk *oclk, *pclk;
	int ret = 0;

	oclk = clk_get(NULL, "cko1");
	pclk = clk_get(NULL, "pll3");

	ret = clk_set_parent(oclk, pclk);
	if (ret)
		printk(KERN_ERR "Can't set parent clock.\n");

	clk_put(pclk);

	ret = clk_set_rate(oclk, 27000000);
	if (ret)
		printk(KERN_ERR "Can't set clock rate.\n");

	ret = clk_enable(oclk);
	if (ret)
		printk(KERN_ERR "Can't enable Output Clock.\n");
}


static void __init mx53_efikasb_board_init(void)
{
	mx53_efikasb_io_init();

	imx53_add_imx_uart(0, NULL);

	imx53_add_ipuv3(&ipu_data);

	imx53_add_vpu();
	imx53_add_ldb(&ldb_data);
	imx53_add_v4l2_output(0);

	imx53_add_imx2_wdt(0, NULL);
	imx53_add_srtc();
	imx53_add_dvfs_core(&efikasb_dvfs_core_data);

	/* I2C */
	/* imx53_add_imx_i2c(0, &mx53_efikasb_i2c_data); */
	mxc_register_device(&i2c_gpio_device, &i2c_gpio_data);

	i2c_register_board_info(0, mxc_i2c0_board_info,
				ARRAY_SIZE(mxc_i2c0_board_info));

	/* SD Interfaces */
	imx53_add_sdhci_esdhc_imx(0, &mx53_efikasb_sd1_data);
	imx53_add_sdhci_esdhc_imx(1, &mx53_efikasb_sd2_data);

	/* USB */
	mx5_set_otghost_vbus_func(mx53_efikasb_usbdr_vbus);
	mx5_usb_dr_init();
	mx5_set_host1_vbus_func(mx53_efikasb_usbh1_vbus);
	mx5_usbh1_init();

	/* Audio */
	imx53_add_imx_ssi(1, &efikasb_ssi_pdata);
	mxc_register_device(&cs42l52_device, &cs42l52_data);

	/* NAND */
	mxc_register_device(&mxc_nandv2_mtd_device, &efikasb_nand_data);

	/*GPU*/
	if (mx53_revision() >= IMX_CHIP_REVISION_2_0)
		gpu_data.z160_revision = 1;
	else
		gpu_data.z160_revision = 0;
	imx53_add_mxc_gpu(&gpu_data);

	/* this call required to release SCC RAM partition held by ROM
	  * during boot, even if SCC2 driver is not part of the image
	  */
	imx53_add_mxc_scc2();

	/* MCLK for camera and audio */
	efikasb_mclk_init();

	/* Power on peripherals */
	mx53_efikasb_periph_power(1);

	/* Power on WLan */
	mx53_efikasb_wlan_power(1);
	/* And do reset... */
	mx53_efikasb_do_periph_reset();
}

static void __init mx53_efikasb_timer_init(void)
{
	mx53_clocks_init(32768, 24000000, 0, 0);
}

static struct sys_timer mx53_efikasb_timer = {
	.init	= mx53_efikasb_timer_init,
};

MACHINE_START(MX53_EFIKASB, "Genesi MX53 Efika SB Board")
	.fixup = fixup_mxc_board,
	.map_io = mx53_map_io,
	.init_early = imx53_init_early,
	.init_irq = mx53_init_irq,
	.timer = &mx53_efikasb_timer,
	.init_machine = mx53_efikasb_board_init,
MACHINE_END
