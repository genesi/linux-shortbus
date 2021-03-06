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
#include <linux/i2c-gpio.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/imx-uart.h>
#include <mach/iomux-mx53.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_dvfs.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/flash.h>
#include <asm/setup.h>

#include "crm_regs.h"
#include "devices-imx53.h"
#include "devices.h"
#include "usb.h"

extern struct clk emi_slow_clk;

/* MX53 Efika SB GPIO PIN configurations */
#define USBDR_OC                IMX_GPIO_NR(4, 14)
#define USBDR_PWREN             IMX_GPIO_NR(4, 15)
#define USBH1_OC                IMX_GPIO_NR(3, 30)
#define USBH1_PWREN             IMX_GPIO_NR(3, 31)
#define CLKO                    IMX_GPIO_NR(1, 5)
#define SD1_WP                  IMX_GPIO_NR(1, 9)
#define SD1_CD                  IMX_GPIO_NR(1, 21)

#define WIFI_PON		IMX_GPIO_NR(1, 0)
#define WIFI_RST		IMX_GPIO_NR(5, 28)
#define CAMERA_PWD		IMX_GPIO_NR(5, 29)

/* GPIO I2C */
#define GPIO_SDA                IMX_GPIO_NR(5, 27)
#define GPIO_SCL                IMX_GPIO_NR(5, 26)

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
	MX53_PAD_GPIO_0__GPIO1_0,        /* WIFI PON */
	MX53_PAD_CSI0_DAT10__GPIO5_28,   /* WIFI RST */
	MX53_PAD_CSI0_DAT11__GPIO5_29,   /* CAMR PWDN */
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
	MX53_PAD_LVDS0_TX3_P__LDB_LVDS0_TX3,  /* Schematic says no */
	MX53_PAD_LVDS0_CLK_P__LDB_LVDS0_CLK,
	MX53_PAD_LVDS0_TX2_P__LDB_LVDS0_TX2,
	MX53_PAD_LVDS0_TX1_P__LDB_LVDS0_TX1,
	MX53_PAD_LVDS0_TX0_P__LDB_LVDS0_TX0,
	/* I2C1 */
	MX53_PAD_CSI0_DAT8__I2C1_SDA,
	MX53_PAD_CSI0_DAT9__I2C1_SCL,
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

static struct ipuv3_fb_platform_data efikasb_fb0_data = {
	.interface_pix_fmt = IPU_PIX_FMT_BGR24,
	.mode_str = "WSVGA",
};

static struct ipuv3_fb_platform_data efikasb_fb1_data = {
	.interface_pix_fmt = IPU_PIX_FMT_BGR24,
	.mode_str = "WSVGA",
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
	.cd_type = ESDHC_CD_CONTROLLER,
	.wp_gpio = SD1_WP,
//	.cd_gpio = SD1_CD,
	.cd_gpio = -EINVAL
};

static const struct esdhc_platform_data mx53_efikasb_sd2_data __initconst = {
	.cd_type = ESDHC_CD_PERMANENT,
};

static void mx53_efikasb_camera_power(int off);
static struct fsl_mxc_camera_platform_data camera_data = {
	.mclk = 27000000,
	.csi = 0,
	.pwdn = &mx53_efikasb_camera_power,
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
	.type = "ov7690",
	.addr = 0x21,
	.platform_data = (void *)&camera_data,
	},
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

struct platform_device mxc_battery = {
	.name = "sppp_power",
	.id = -1,
};

struct platform_device mxc_backlight = {
	.name = "sppp_backlight",
	.id = -1,
};

struct platform_device mxc_rtc = {
	.name = "sppp_rtc",
	.id = -1,
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

static void mx53_efikasb_wifi_power(bool on)
{
	if (on)
		gpio_set_value(WIFI_PON, 1);
	else
		gpio_set_value(WIFI_PON, 0);
}

static void mx53_efikasb_camera_power(int off)
{
	if (off)
		gpio_set_value(CAMERA_PWD, 1);
	else
		gpio_set_value(CAMERA_PWD, 0);
}

static void mx53_efikasb_wifi_reset(void)
{
	gpio_set_value(WIFI_RST, 1);
	msleep(50);
	gpio_set_value(WIFI_RST, 0);
	msleep(1);
	gpio_set_value(WIFI_RST, 1);
	msleep(30);
}

static struct imx_ssi_platform_data efikasb_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN | IMX_SSI_NET | IMX_SSI_USE_I2S_SLAVE,
};


static struct mxc_gpu_platform_data gpu_data __initdata;
static struct mxc_vpu_platform_data vpu_data __initdata;
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
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int gpu_mem = SZ_32M + SZ_16M;
	int fb_mem = SZ_16M;
	int vpu_mem = SZ_32M;
	int sys_mem = 0;

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			break;
		}
	}

	sys_mem = total_mem - gpu_mem - fb_mem - vpu_mem;

	if (mem_tag) {
		mem_tag->u.mem.size = sys_mem;

		gpu_data.reserved_mem_base = mem_tag->u.mem.start + sys_mem;
		gpu_data.reserved_mem_size = gpu_mem;

		vpu_data.reserved_mem_base = gpu_data.reserved_mem_base
					+ gpu_data.reserved_mem_size;
		vpu_data.reserved_mem_size = vpu_mem;

		efikasb_fb0_data.res_base = vpu_data.reserved_mem_base
					+ vpu_data.reserved_mem_size;
		efikasb_fb0_data.res_size = fb_mem;
		efikasb_fb1_data.res_base = efikasb_fb0_data.res_base
					+ efikasb_fb0_data.res_size;
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

	/* USB PWR enable */
	gpio_request(USBH1_PWREN, "usb-pwr1");
	gpio_direction_output(USBH1_PWREN, 0);

	gpio_request(USBDR_PWREN, "usb-pwr0");
	gpio_direction_output(USBDR_PWREN, 0);

	/* Wifi Power On signal */
	gpio_request(WIFI_PON, "wifi-power");
	gpio_direction_output(WIFI_PON, 0);

	/* Wifi Reset */
	gpio_request(WIFI_RST, "wifi-reset");
	gpio_direction_output(WIFI_RST, 0);

	/* Camera Power Down signal */
	gpio_request(CAMERA_PWD, "camera-power");
	gpio_direction_output(CAMERA_PWD, 1);
}

/* Sets up the MCLK for audio and camera */
static void efikasb_mclk_init(void)
{

	struct clk *oclk, *pclk;
	int ret = 0;
	unsigned long rate;

	gpio_set_value(CAMERA_PWD, 1);
	msleep(20);

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

	rate = clk_get_rate(oclk);

	msleep(10);
	gpio_set_value(CAMERA_PWD, 0);

/*	printk(KERN_ERR "Clock rate %ld\n",  rate); */
}

static void __init mx53_efikasb_board_init(void)
{
	mx53_efikasb_io_init();

	imx53_add_imx_uart(0, NULL);

	imx53_add_ipuv3(&ipu_data);

	imx53_add_vpu(&vpu_data);
	imx53_add_ldb(&ldb_data);
	imx53_add_v4l2_output(0);
	imx53_add_v4l2_capture(0);

	imx53_add_imx2_wdt(0, NULL);
	imx53_add_dvfs_core(&efikasb_dvfs_core_data);

	/* I2C */
	imx53_add_imx_i2c(0, &mx53_efikasb_i2c_data);

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

	/* Battery */
	mxc_register_device(&mxc_battery, NULL);

	/* Backlight */
	mxc_register_device(&mxc_backlight, NULL);

	/* Real-Time Clock */
	mxc_register_device(&mxc_rtc, NULL);

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

	/* Power on camera */
	mx53_efikasb_camera_power(0);
	/* Power on wlan */
	mx53_efikasb_wifi_power(1);
	/* And do reset... */
	mx53_efikasb_wifi_reset();
}

static void __init mx53_efikasb_timer_init(void)
{
	mx53_clocks_init(32768, 24000000, 0, 0);
}

static struct sys_timer mx53_efikasb_timer = {
	.init	= mx53_efikasb_timer_init,
};

MACHINE_START(MX53_EFIKASB, "Genesi Efika MX (Slimbook)")
	.fixup = fixup_mxc_board,
	.map_io = mx53_map_io,
	.init_early = imx53_init_early,
	.init_irq = mx53_init_irq,
	.timer = &mx53_efikasb_timer,
	.init_machine = mx53_efikasb_board_init,
MACHINE_END
