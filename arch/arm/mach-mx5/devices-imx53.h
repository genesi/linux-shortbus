/*
 * Copyright (C) 2010 Yong Shen. <Yong.Shen@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <mach/mx53.h>
#include <mach/devices-common.h>

extern const struct imx_fec_data imx53_fec_data;
#define imx53_add_fec(pdata)   \
	imx_add_fec(&imx53_fec_data, pdata)

extern const struct imx_imx_uart_1irq_data imx53_imx_uart_data[];
#define imx53_add_imx_uart(id, pdata)	\
	imx_add_imx_uart_1irq(&imx53_imx_uart_data[id], pdata)


extern const struct imx_imx_i2c_data imx53_imx_i2c_data[];
#define imx53_add_imx_i2c(id, pdata)	\
	imx_add_imx_i2c(&imx53_imx_i2c_data[id], pdata)

extern const struct imx_sdhci_esdhc_imx_data imx53_sdhci_esdhc_imx_data[];
#define imx53_add_sdhci_esdhc_imx(id, pdata)	\
	imx_add_sdhci_esdhc_imx(&imx53_sdhci_esdhc_imx_data[id], pdata)

extern const struct imx_spi_imx_data imx53_ecspi_data[];
#define imx53_add_ecspi(id, pdata)	\
	imx_add_spi_imx(&imx53_ecspi_data[id], pdata)

extern const struct imx_imx2_wdt_data imx53_imx2_wdt_data[];
#define imx53_add_imx2_wdt(id, pdata)	\
	imx_add_imx2_wdt(&imx53_imx2_wdt_data[id])

extern const struct imx_imx_ssi_data imx53_imx_ssi_data[];
#define imx53_add_imx_ssi(id, pdata)	\
	imx_add_imx_ssi(&imx53_imx_ssi_data[id], pdata)

extern const struct imx_imx_keypad_data imx53_imx_keypad_data;
#define imx53_add_imx_keypad(pdata)	\
	imx_add_imx_keypad(&imx53_imx_keypad_data, pdata)

extern const struct imx_srtc_data imx53_imx_srtc_data __initconst;
#define imx53_add_srtc()	\
	imx_add_srtc(&imx53_imx_srtc_data)

extern const struct imx_iim_data imx53_imx_iim_data __initconst;
#define imx53_add_iim(pdata) \
	imx_add_iim(&imx53_imx_iim_data, pdata)

extern const struct imx_ahci_imx_data imx53_ahci_imx_data __initconst;
#define imx53_add_ahci_imx(id, pdata)   \
	imx_add_ahci_imx(&imx53_ahci_imx_data, pdata)

extern const struct imx_ipuv3_data imx53_ipuv3_data __initconst;
#define imx53_add_ipuv3(id, pdata)	imx_add_ipuv3(id, &imx53_ipuv3_data, pdata)
#define imx53_add_ipuv3fb(id, pdata)	imx_add_ipuv3_fb(id, pdata)

extern const struct imx_tve_data imx53_tve_data __initconst;
#define imx53_add_tve(pdata)	\
	imx_add_tve(&imx53_tve_data, pdata)

extern const struct imx_ldb_data imx53_ldb_data __initconst;
#define imx53_add_ldb(pdata) \
	imx_add_ldb(&imx53_ldb_data, pdata);

extern const struct imx_mxc_pwm_data imx53_mxc_pwm_data[] __initconst;
#define imx53_add_mxc_pwm(id)	\
	imx_add_mxc_pwm(&imx53_mxc_pwm_data[id])

#define imx53_add_mxc_pwm_backlight(id, pdata)			\
	platform_device_register_resndata(NULL, "pwm-backlight",\
			id, NULL, 0, pdata, sizeof(*pdata));

#define imx53_add_v4l2_output(id)	\
	platform_device_register_resndata(NULL, "mxc_v4l2_output",\
			id, NULL, 0, NULL, 0);

extern const struct imx_vpu_data imx53_vpu_data __initconst;
#define imx53_add_vpu()	imx_add_vpu(&imx53_vpu_data)
