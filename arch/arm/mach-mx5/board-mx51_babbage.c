/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2009-2010 Amit Kucheria <amit.kucheria@canonical.com>
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fsl_devices.h>
#include <linux/fec.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/iomux-mx51.h>
#include <mach/mxc_ehci.h>
#include <mach/ipu-v3.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "devices-imx51.h"
#include "devices.h"
#include "crm_regs.h"
#include "cpu_op-mx51.h"
#include "usb.h"

#define BABBAGE_USB_HUB_RESET	IMX_GPIO_NR(1, 7)
#define BABBAGE_USBH1_STP	IMX_GPIO_NR(1, 27)
#define BABBAGE_USBCLK_EN_B	IMX_GPIO_NR(2, 1)
#define BABBAGE_OSC_EN_B	IMX_GPIO_NR(2, 2)
#define BABBAGE_PHY_RESET	IMX_GPIO_NR(2, 5)
#define BABBAGE_VGA_RESET	IMX_GPIO_NR(2, 13)
#define BABBAGE_FEC_PHY_RESET	IMX_GPIO_NR(2, 14)
#define BABBAGE_POWER_KEY	IMX_GPIO_NR(2, 21)
#define BABBAGE_ECSPI1_CS0	IMX_GPIO_NR(4, 24)
#define BABBAGE_ECSPI1_CS1	IMX_GPIO_NR(4, 25)
#define MX51_BBG_SD1_CD         IMX_GPIO_NR(1, 0)
#define MX51_BBG_SD1_WP         IMX_GPIO_NR(1, 1)
#define MX51_BBG_SD2_CD         IMX_GPIO_NR(1, 6)
#define MX51_BBG_SD2_WP         IMX_GPIO_NR(1, 5)
#define BABBAGE_AUDAMP_STBY	IMX_GPIO_NR(2, 17)
#define BABBAGE_26M_OSC_EN	IMX_GPIO_NR(3, 1)
#define BABBAGE_HEADPHONE_DET	IMX_GPIO_NR(3, 26)
#define BABBAGE_AUDIO_CLK_EN	IMX_GPIO_NR(4, 26)

#define BABBAGE_LVDS_POWER_DOWN		IMX_GPIO_NR(3, 3)	/* GPIO_3_3 */
#define BABBAGE_DISP_BRIGHTNESS_CTL	IMX_GPIO_NR(3, 4)	/* GPIO_3_4 */
#define BABBAGE_DVI_RESET		IMX_GPIO_NR(3, 5)	/* GPIO_3_5 */
#define BABBAGE_DVI_POWER		IMX_GPIO_NR(3, 6)	/* GPIO_3_6 */
#define BABBAGE_HEADPHONE_DET		IMX_GPIO_NR(3, 26)	/* GPIO_3_26 */
#define BABBAGE_DVI_DET			IMX_GPIO_NR(3, 28)	/* GPIO_3_28 */

#define BABBAGE_LCD_3V3_ON		IMX_GPIO_NR(4, 9)	/* GPIO_4_9 */
#define BABBAGE_LCD_5V_ON		IMX_GPIO_NR(4, 10)	/* GPIO_4_10 */
#define BABBAGE_DVI_I2C_EN		IMX_GPIO_NR(4, 14)	/* GPIO_4_14 */

/* USB_CTRL_1 */
#define MX51_USB_CTRL_1_OFFSET			0x10
#define MX51_USB_CTRL_UH1_EXT_CLK_EN		(1 << 25)

#define	MX51_USB_PLLDIV_12_MHZ		0x00
#define	MX51_USB_PLL_DIV_19_2_MHZ	0x01
#define	MX51_USB_PLL_DIV_24_MHZ	0x02

static struct gpio_keys_button babbage_buttons[] = {
	{
		.gpio		= BABBAGE_POWER_KEY,
		.code		= BTN_0,
		.desc		= "PWR",
		.active_low	= 1,
		.wakeup		= 1,
	},
};

static const struct gpio_keys_platform_data imx_button_data __initconst = {
	.buttons	= babbage_buttons,
	.nbuttons	= ARRAY_SIZE(babbage_buttons),
};

static iomux_v3_cfg_t mx51babbage_pads[] = {
	/* UART1 */
	MX51_PAD_UART1_RXD__UART1_RXD,
	MX51_PAD_UART1_TXD__UART1_TXD,
	MX51_PAD_UART1_RTS__UART1_RTS,
	MX51_PAD_UART1_CTS__UART1_CTS,

	/* UART2 */
	MX51_PAD_UART2_RXD__UART2_RXD,
	MX51_PAD_UART2_TXD__UART2_TXD,

	/* UART3 */
	MX51_PAD_EIM_D25__UART3_RXD,
	MX51_PAD_EIM_D26__UART3_TXD,
	MX51_PAD_EIM_D27__UART3_RTS,
	MX51_PAD_EIM_D24__UART3_CTS,

	/* I2C1 */
	MX51_PAD_EIM_D16__I2C1_SDA,
	MX51_PAD_EIM_D19__I2C1_SCL,

	/* I2C2 */
	MX51_PAD_KEY_COL4__I2C2_SCL,
	MX51_PAD_KEY_COL5__I2C2_SDA,

	/* HSI2C */
	MX51_PAD_I2C1_CLK__I2C1_CLK,
	MX51_PAD_I2C1_DAT__I2C1_DAT,

	/* USB HOST1 */
	MX51_PAD_USBH1_CLK__USBH1_CLK,
	MX51_PAD_USBH1_DIR__USBH1_DIR,
	MX51_PAD_USBH1_NXT__USBH1_NXT,
	MX51_PAD_USBH1_DATA0__USBH1_DATA0,
	MX51_PAD_USBH1_DATA1__USBH1_DATA1,
	MX51_PAD_USBH1_DATA2__USBH1_DATA2,
	MX51_PAD_USBH1_DATA3__USBH1_DATA3,
	MX51_PAD_USBH1_DATA4__USBH1_DATA4,
	MX51_PAD_USBH1_DATA5__USBH1_DATA5,
	MX51_PAD_USBH1_DATA6__USBH1_DATA6,
	MX51_PAD_USBH1_DATA7__USBH1_DATA7,

	/* USB HUB reset line*/
	MX51_PAD_GPIO1_7__GPIO1_7,

	/* USB PHY & CLK */
	MX51_PAD_EIM_D17__GPIO2_1,
	MX51_PAD_EIM_D18__GPIO2_2,
	MX51_PAD_EIM_D21__GPIO2_5,
	MX51_PAD_DI1_PIN12__GPIO3_1,
	MX51_PAD_OWIRE_LINE__SPDIF_OUT,

	/* FEC */
	MX51_PAD_EIM_EB2__FEC_MDIO,
	MX51_PAD_EIM_EB3__FEC_RDATA1,
	MX51_PAD_EIM_CS2__FEC_RDATA2,
	MX51_PAD_EIM_CS3__FEC_RDATA3,
	MX51_PAD_EIM_CS4__FEC_RX_ER,
	MX51_PAD_EIM_CS5__FEC_CRS,
	MX51_PAD_NANDF_RB2__FEC_COL,
	MX51_PAD_NANDF_RB3__FEC_RX_CLK,
	MX51_PAD_NANDF_D9__FEC_RDATA0,
	MX51_PAD_NANDF_D8__FEC_TDATA0,
	MX51_PAD_NANDF_CS2__FEC_TX_ER,
	MX51_PAD_NANDF_CS3__FEC_MDC,
	MX51_PAD_NANDF_CS4__FEC_TDATA1,
	MX51_PAD_NANDF_CS5__FEC_TDATA2,
	MX51_PAD_NANDF_CS6__FEC_TDATA3,
	MX51_PAD_NANDF_CS7__FEC_TX_EN,
	MX51_PAD_NANDF_RDY_INT__FEC_TX_CLK,

	/* FEC PHY reset line */
	MX51_PAD_EIM_A20__GPIO2_14,

	MX51_PAD_GPIO1_0__GPIO1_0,
	MX51_PAD_GPIO1_1__GPIO1_1,
	MX51_PAD_GPIO1_5__GPIO1_5,
	MX51_PAD_GPIO1_6__GPIO1_6,
	/* SD 1 */
	MX51_PAD_SD1_CMD__SD1_CMD,
	MX51_PAD_SD1_CLK__SD1_CLK,
	MX51_PAD_SD1_DATA0__SD1_DATA0,
	MX51_PAD_SD1_DATA1__SD1_DATA1,
	MX51_PAD_SD1_DATA2__SD1_DATA2,
	MX51_PAD_SD1_DATA3__SD1_DATA3,

	/* SD 2 */
	MX51_PAD_SD2_CMD__SD2_CMD,
	MX51_PAD_SD2_CLK__SD2_CLK,
	MX51_PAD_SD2_DATA0__SD2_DATA0,
	MX51_PAD_SD2_DATA1__SD2_DATA1,
	MX51_PAD_SD2_DATA2__SD2_DATA2,
	MX51_PAD_SD2_DATA3__SD2_DATA3,

	/* eCSPI1 */
	MX51_PAD_CSPI1_MISO__ECSPI1_MISO,
	MX51_PAD_CSPI1_MOSI__ECSPI1_MOSI,
	MX51_PAD_CSPI1_SCLK__ECSPI1_SCLK,
	MX51_PAD_CSPI1_SS0__ECSPI1_SS0,
	MX51_PAD_CSPI1_SS0__GPIO4_24,
	MX51_PAD_CSPI1_SS1__GPIO4_25,
	MX51_PAD_CSPI1_RDY__GPIO4_26,
	MX51_PAD_CSPI1_SS1__ECSPI1_SS1,

	/* Display */
	MX51_PAD_EIM_A19__GPIO2_13,
	MX51_PAD_DI1_D0_CS__GPIO3_3,
	MX51_PAD_DISPB2_SER_DIN__GPIO3_5,
	MX51_PAD_DISPB2_SER_DIO__GPIO3_6,
	MX51_PAD_NANDF_D14__GPIO3_26,
	MX51_PAD_NANDF_D12__GPIO3_28,
	MX51_PAD_CSI2_D12__GPIO4_9,
	MX51_PAD_CSI2_D13__GPIO4_10,
	MX51_PAD_CSI2_HSYNC__GPIO4_14,

	MX51_PAD_DI_GP4__DI2_PIN15,
	MX51_PAD_GPIO1_2__PWM1_PWMO,

#ifdef CONFIG_FB_MXC_CLAA_WVGA_SYNC_PANEL
	MX51_PAD_DISP1_DAT22__DISP2_DAT16,
	MX51_PAD_DISP1_DAT23__DISP2_DAT17,

	MX51_PAD_DI1_D1_CS__GPIO3_4,
#endif
	MX51_PAD_EIM_LBA__GPIO3_1,
	MX51_PAD_AUD3_BB_TXD__AUD3_TXD,
	MX51_PAD_AUD3_BB_RXD__AUD3_RXD,
	MX51_PAD_AUD3_BB_CK__AUD3_TXC,
	MX51_PAD_AUD3_BB_FS__AUD3_TXFS,

	MX51_PAD_OWIRE_LINE__SPDIF_OUT,
};

/* Serial ports */
static const struct imxuart_platform_data uart_pdata __initconst = {
	.flags = IMXUART_HAVE_RTSCTS,
};

static const struct imxi2c_platform_data babbage_i2c_data __initconst = {
	.bitrate = 100000,
};

static struct imxi2c_platform_data babbage_hsi2c_data = {
	.bitrate = 400000,
};

static const struct esdhc_platform_data mx51_bbg_sd1_data __initconst = {
	.wp_gpio = MX51_BBG_SD1_WP,
	.cd_gpio = MX51_BBG_SD1_CD,
};

static const struct esdhc_platform_data mx51_bbg_sd2_data __initconst = {
	.wp_gpio = MX51_BBG_SD2_WP,
	.cd_gpio = MX51_BBG_SD2_CD,
};

static void babbage_suspend_enter(void)
{
}

static void babbage_suspend_exit(void)
{
	/*clear the EMPGC0/1 bits */
	__raw_writel(0, MXC_SRPG_EMPGC0_SRPGCR);
	__raw_writel(0, MXC_SRPG_EMPGC1_SRPGCR);
}

static struct mxc_pm_platform_data babbage_pm_data = {
	.suspend_enter = babbage_suspend_enter,
	.suspend_exit = babbage_suspend_exit,
};

static inline void babbage_usbhub_reset(void)
{
	int ret;

	/* Bring USB hub out of reset */
	ret = gpio_request(BABBAGE_USB_HUB_RESET, "GPIO1_7");
	if (ret) {
		printk(KERN_ERR"failed to get GPIO_USB_HUB_RESET: %d\n", ret);
		return;
	}
	gpio_direction_output(BABBAGE_USB_HUB_RESET, 0);

	/* USB HUB RESET - De-assert USB HUB RESET_N */
	msleep(1);
	gpio_set_value(BABBAGE_USB_HUB_RESET, 0);
	msleep(1);
	gpio_set_value(BABBAGE_USB_HUB_RESET, 1);
}

static inline void babbage_fec_reset(void)
{
	int ret;

	/* reset FEC PHY */
	ret = gpio_request_one(BABBAGE_FEC_PHY_RESET,
					GPIOF_OUT_INIT_LOW, "fec-phy-reset");
	if (ret) {
		printk(KERN_ERR"failed to get GPIO_FEC_PHY_RESET: %d\n", ret);
		return;
	}
	msleep(1);
	gpio_set_value(BABBAGE_FEC_PHY_RESET, 1);
}

static struct spi_board_info mx51_babbage_spi_board_info[] __initdata = {
	{
		.modalias = "mtd_dataflash",
		.max_speed_hz = 25000000,
		.bus_num = 0,
		.chip_select = 1,
		.mode = SPI_MODE_0,
		.platform_data = NULL,
	},
};

static int mx51_babbage_spi_cs[] = {
	BABBAGE_ECSPI1_CS0,
	BABBAGE_ECSPI1_CS1,
};

static const struct spi_imx_master mx51_babbage_spi_pdata __initconst = {
	.chipselect     = mx51_babbage_spi_cs,
	.num_chipselect = ARRAY_SIZE(mx51_babbage_spi_cs),
};

static struct fb_videomode video_modes[] = {
	{
	 /*MITSUBISHI LVDS panel */
	 "XGA", 60, 1024, 768, 15385,
	 220, 40,
	 21, 7,
	 60, 10,
	 0,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	 "CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

static struct ipuv3_fb_platform_data bbg_fb_di0_data = {
	.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	.mode_str = "1024x768M-16@60",
	.modes = video_modes,
	.num_modes = ARRAY_SIZE(video_modes),
};

static struct ipuv3_fb_platform_data bbg_fb_di1_data = {
	.interface_pix_fmt = IPU_PIX_FMT_RGB565,
	.mode_str = "CLAA-WVGA",
	.modes = video_modes,
	.num_modes = ARRAY_SIZE(video_modes),
};

static struct imx_ipuv3_platform_data ipu_data = {
	.rev = 2,
	.fb_head0_platform_data = &bbg_fb_di0_data,
	.fb_head1_platform_data = &bbg_fb_di1_data,
	.primary_di = MXC_PRI_DI0,
};

static struct platform_pwm_backlight_data bbg_pwm_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 128,
	.pwm_period_ns = 78770,
};

static struct fsl_mxc_tve_platform_data tve_data = {
	.dac_reg = "VVIDEO",
};

static void vga_reset(void)
{

	gpio_set_value(BABBAGE_VGA_RESET, 0);
	msleep(50);
	/* do reset */
	gpio_set_value(BABBAGE_VGA_RESET, 1);
	msleep(10);		/* tRES >= 50us */
	gpio_set_value(BABBAGE_VGA_RESET, 0);
}

static struct fsl_mxc_lcd_platform_data vga_data = {
	.core_reg = "VCAM",
	.io_reg = "VGEN3",
	.analog_reg = "VAUDIO",
	.reset = vga_reset,
};

static void ddc_dvi_init(void)
{
	/* enable DVI I2C */
	gpio_set_value(BABBAGE_DVI_I2C_EN, 1);
}

static int ddc_dvi_update(void)
{
	/* DVI cable state */
	if (gpio_get_value(BABBAGE_DVI_DET) == 1)
		return 1;
	else
		return 0;
}

static struct fsl_mxc_ddc_platform_data bbg_ddc_dvi_data = {
	.di = 0,
	.init = ddc_dvi_init,
	.update = ddc_dvi_update,
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
	 .type = "sgtl5000",
	 .addr = 0x0a,
	 },
	{
	 .type = "mxc_ddc",
	 .addr = 0x50,
	 .irq = gpio_to_irq(BABBAGE_DVI_DET),
	 .platform_data = &bbg_ddc_dvi_data,
	 },
};

static struct i2c_board_info mxc_i2c_hs_board_info[] __initdata = {
	{
	 .type = "ch7026",
	 .addr = 0x75,
	 .platform_data = &vga_data,
	 },
};

static struct mxc_gpu_platform_data gpu_data __initdata;

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int left_mem = 0;
	int gpu_mem = SZ_64M;
	int fb_mem = 0;
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

		/*reserve memory for gpu*/
		gpu_data.reserved_mem_base =
				mem_tag->u.mem.start + left_mem;
		gpu_data.reserved_mem_size = gpu_mem;

		/* reserver memory for fb */
		bbg_fb_di0_data.res_base = gpu_data.reserved_mem_base
					+ gpu_data.reserved_mem_size;
		bbg_fb_di0_data.res_size = fb_mem;
		bbg_fb_di1_data.res_base = bbg_fb_di0_data.res_base;
		bbg_fb_di1_data.res_size = bbg_fb_di0_data.res_size;
	}
}

static struct imx_ssi_platform_data bbg_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

extern int mx51_babbage_init_mc13892(void);
static int bbg_sgtl5000_init(void)
{
	gpio_request(BABBAGE_AUDAMP_STBY, "audio_amp");
	gpio_direction_input(BABBAGE_AUDAMP_STBY);

	/* Enable OSC_CKIH1_EN for audio */
	gpio_request(BABBAGE_AUDIO_CLK_EN, "audio_clk");
	gpio_direction_output(BABBAGE_AUDIO_CLK_EN, 0);
	gpio_set_value(BABBAGE_AUDIO_CLK_EN, 0);

	return 0;
}

static struct mxc_audio_platform_data bbg_audio_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.init = bbg_sgtl5000_init,
	.sysclk = 26000000,
	.hp_gpio = BABBAGE_HEADPHONE_DET,
	.hp_active_low = 1,
};

static struct platform_device bbg_audio_device = {
	.name = "imx-sgtl5000",
};

static void __init mx51_bbg_init_usb(void)
{
	imx_otg_base = MX51_IO_ADDRESS(MX51_OTG_BASE_ADDR);

	/* Bring USB hub out of reset */
	gpio_request(BABBAGE_USB_HUB_RESET, "GPIO1_7");
	gpio_direction_output(BABBAGE_USB_HUB_RESET, 0);
	/* USB HUB RESET - De-assert USB HUB RESET_N */
	msleep(1);
	gpio_set_value(BABBAGE_USB_HUB_RESET, 0);
	msleep(1);
	gpio_set_value(BABBAGE_USB_HUB_RESET, 1);

	/* Enable 26M_OSC */
	gpio_request(BABBAGE_26M_OSC_EN, "26M-OSC-CLK");
	gpio_direction_output(BABBAGE_26M_OSC_EN, 1);

	/* OSC_EN */
	gpio_request(BABBAGE_USBCLK_EN_B, "usb-clk_en_b");
	gpio_direction_output(BABBAGE_USBCLK_EN_B, 0);

	/* OSC_EN */
	gpio_request(BABBAGE_OSC_EN_B, "osc-en");
	gpio_direction_output(BABBAGE_OSC_EN_B, 1);

	/* De-assert USB PHY RESETB */
	gpio_request(BABBAGE_PHY_RESET, "phy_reset");
	gpio_direction_output(BABBAGE_PHY_RESET, 1);

	mx5_usb_dr_init();
	mx5_usbh1_init();
}

static struct mxc_spdif_platform_data mxc_spdif_data = {
	.spdif_tx = 1,
	.spdif_rx = 0,
	.spdif_clk_44100 = 0,	/* spdif_ext_clk source for 44.1KHz */
	.spdif_clk_48000 = 7,	/* audio osc source */
	.spdif_clkid = 0,
	.spdif_clk = NULL,	/* spdif bus clk */
};

/*
 * Board specific initialization.
 */
static void __init mx51_babbage_init(void)
{
	iomux_v3_cfg_t power_key = _MX51_PAD_EIM_A27__GPIO2_21 |
		MUX_PAD_CTRL(PAD_CTL_SRE_FAST | PAD_CTL_DSE_HIGH | PAD_CTL_PUS_100K_UP);

	mxc_iomux_v3_setup_multiple_pads(mx51babbage_pads,
					ARRAY_SIZE(mx51babbage_pads));

	mxc_spdif_data.spdif_core_clk = clk_get(NULL, "spdif_xtal_clk");
	clk_put(mxc_spdif_data.spdif_core_clk);

	imx51_add_imx_uart(0, &uart_pdata);
	imx51_add_imx_uart(1, &uart_pdata);
	imx51_add_imx_uart(2, &uart_pdata);

	imx51_add_srtc();

	babbage_fec_reset();
	imx51_add_fec(NULL);

	imx51_add_ipuv3(&ipu_data);
	imx51_add_vpu();
	imx51_add_tve(&tve_data);
	imx51_add_v4l2_output(0);

	imx51_add_mxc_pwm(0);
	imx51_add_mxc_pwm_backlight(0, &bbg_pwm_backlight_data);

	/* Set the PAD settings for the pwr key. */
	mxc_iomux_v3_setup_pad(power_key);
	imx51_add_gpio_keys(&imx_button_data);

	imx51_add_imx_i2c(0, &babbage_i2c_data);
	imx51_add_imx_i2c(1, &babbage_i2c_data);

	imx51_add_spdif(&mxc_spdif_data);
	imx51_add_spdif_dai();
	imx51_add_spdif_audio_device();

	mxc_register_device(&mxc_hsi2c_device, &babbage_hsi2c_data);
	mxc_register_device(&mxc_pm_device, &babbage_pm_data);
	i2c_register_board_info(1, mxc_i2c1_board_info,
				ARRAY_SIZE(mxc_i2c1_board_info));

	vga_data.core_reg = NULL;
	vga_data.io_reg = NULL;
	vga_data.analog_reg = NULL;

	i2c_register_board_info(3, mxc_i2c_hs_board_info,
				ARRAY_SIZE(mxc_i2c_hs_board_info));

	imx51_add_sdhci_esdhc_imx(0, &mx51_bbg_sd1_data);
	imx51_add_sdhci_esdhc_imx(1, &mx51_bbg_sd2_data);

	spi_register_board_info(mx51_babbage_spi_board_info,
		ARRAY_SIZE(mx51_babbage_spi_board_info));

	imx51_add_ecspi(0, &mx51_babbage_spi_pdata);
	imx51_add_imx2_wdt(0, NULL);
	imx51_add_mxc_gpu(&gpu_data);

	/* this call required to release IRAM partition held by ROM during boot,
	  * even if SCC2 driver is not part of the image
	  */
	imx51_add_mxc_scc2();

	/* USB HOST and OTG */
	mx51_bbg_init_usb();

	if (mx51_revision() >= IMX_CHIP_REVISION_3_0) {
		/* DVI_I2C_ENB = 0 tristates the DVI I2C level shifter */
		gpio_request(BABBAGE_DVI_I2C_EN, "dvi-i2c-en");
		gpio_direction_output(BABBAGE_DVI_I2C_EN, 0);
	}

	/* Deassert VGA reset to free i2c bus */
	gpio_request(BABBAGE_VGA_RESET, "vga-reset");
	gpio_direction_output(BABBAGE_VGA_RESET, 1);

	/* LCD related gpio */
	gpio_request(BABBAGE_DISP_BRIGHTNESS_CTL, "disp-brightness-ctl");
	gpio_request(BABBAGE_LVDS_POWER_DOWN, "lvds-power-down");
	gpio_request(BABBAGE_LCD_3V3_ON, "lcd-3v3-on");
	gpio_request(BABBAGE_LCD_5V_ON, "lcd-5v-on");
	gpio_direction_output(BABBAGE_DISP_BRIGHTNESS_CTL, 0);
	gpio_direction_output(BABBAGE_LVDS_POWER_DOWN, 0);
	gpio_direction_output(BABBAGE_LCD_3V3_ON, 0);
	gpio_direction_output(BABBAGE_LCD_5V_ON, 0);

	/* DI0-LVDS */
	gpio_set_value(BABBAGE_LVDS_POWER_DOWN, 0);
	msleep(1);
	gpio_set_value(BABBAGE_LVDS_POWER_DOWN, 1);
	gpio_set_value(BABBAGE_LCD_3V3_ON, 1);
	gpio_set_value(BABBAGE_LCD_5V_ON, 1);

	/* DVI Detect */
	gpio_request(BABBAGE_DVI_DET, "dvi-detect");
	gpio_direction_input(BABBAGE_DVI_DET);
	/* DVI Reset - Assert for i2c disabled mode */
	gpio_request(BABBAGE_DVI_RESET, "dvi-reset");
	gpio_direction_output(BABBAGE_DVI_RESET, 0);
	/* DVI Power-down */
	gpio_request(BABBAGE_DVI_POWER, "dvi-power");
	gpio_direction_output(BABBAGE_DVI_POWER, 1);

	gpio_request(BABBAGE_26M_OSC_EN, "26M-OSC-CLK");
	gpio_direction_output(BABBAGE_26M_OSC_EN, 1);

	/* OSC_EN */
	gpio_request(BABBAGE_OSC_EN_B, "osc-en");
	gpio_direction_output(BABBAGE_OSC_EN_B, 1);

	/* WVGA Reset */
	gpio_set_value(BABBAGE_DISP_BRIGHTNESS_CTL, 1);

	mx51_babbage_init_mc13892();
	mxc_register_device(&bbg_audio_device, &bbg_audio_data);
	imx51_add_imx_ssi(1, &bbg_ssi_pdata);

}

static void __init mx51_babbage_timer_init(void)
{
	mx51_clocks_init(32768, 24000000, 22579200, 0);
}

static struct sys_timer mx51_babbage_timer = {
	.init = mx51_babbage_timer_init,
};

static const char *mx51_babbage_dt_match[] __initdata = {
	"fsl,mx51-babbage",
	NULL
};

MACHINE_START(MX51_BABBAGE, "Freescale MX51 Babbage Board")
	/* Maintainer: Amit Kucheria <amit.kucheria@canonical.com> */
	.fixup = fixup_mxc_board,
	.boot_params = MX51_PHYS_OFFSET + 0x100,
	.map_io = mx51_map_io,
	.init_early = imx51_init_early,
	.init_irq = mx51_init_irq,
	.timer = &mx51_babbage_timer,
	.init_machine = mx51_babbage_init,
	.dt_compat = mx51_babbage_dt_match,
MACHINE_END
