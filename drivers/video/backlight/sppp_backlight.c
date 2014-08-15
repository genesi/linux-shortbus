/*
 *  SPPP Backlight Driver
 *
 *  Copyright (c) 2014 Genesi USA, Inc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/sppp.h>

#define SPPP_BLK_ID_MASK 31
#define BACKLIGHT_INTENSITY_MAX 6

static int sppp_backlight_intensity;
static int sppp_backlight_vals[BACKLIGHT_INTENSITY_MAX+1] = 
	{0, 1, 6, 12, 18, 24, 31};

static struct sppp_client sppp_backlight_client;
static struct backlight_device *sppp_backlight_device;
static struct generic_bl_info *bl_machinfo;


/*************************************************************
 * STM SPPP Functions - naming conventions                   *
 *     'get'          - SPPP callback function               *
 *     'request'      - Send message expecting a response    *
 *     'do'           - Send message expecting no response   *
 *************************************************************/

static void sppp_get_data(const sppp_rx_t *packet)
{
	printk(KERN_INFO "Backlight should not to receive STM messages!\n");
	return;
}

static void sppp_do_set_backlight(uint8_t pwm)
{
	sppp_client_send_start(BACKLIGHT, SPPP_BKL_ID);
	sppp_client_send_data(BACKLIGHT, pwm);
	sppp_client_send_stop(BACKLIGHT);
}


/*
 * Kernel Backlight API
 */

static int sppp_backlight_get_intensity(struct backlight_device *bd)
{
	return sppp_backlight_intensity;
}

static int sppp_backlight_send_intensity(struct backlight_device *bd)
{
	int intensity = min(BACKLIGHT_INTENSITY_MAX, bd->props.brightness);
	uint8_t pwm = sppp_backlight_vals[intensity];

	/* TODO: Check properties power and state */

	sppp_do_set_backlight(pwm);
	sppp_backlight_intensity = intensity;

	/* TODO: kick_battery ? */

	return 0;
}

static const struct backlight_ops sppp_backlight_ops = {
	.options = 0,
	.get_brightness = sppp_backlight_get_intensity,
	.update_status = sppp_backlight_send_intensity,
};


/*
 * Device driver setup
 */

static int sppp_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct generic_bl_info *machinfo = pdev->dev.platform_data;
	struct backlight_device *bd;

	printk(KERN_ERR "%s", __func__);

	/* Set initial backlight properties */
	memset(&props, 0, sizeof(struct backlight_properties));
	props.power = 0;
	props.max_brightness = BACKLIGHT_INTENSITY_MAX; /* TODO:  Get from platform data */
	props.brightness = (sppp_backlight_intensity = BACKLIGHT_INTENSITY_MAX);

	bd = backlight_device_register("sppp_backlight", &pdev->dev, NULL,
		&sppp_backlight_ops, &props);

	if (IS_ERR (bd))
		return PTR_ERR (bd);

	platform_set_drvdata(pdev, bd);
	/* sppp_backlight_status_update(bd) */

	sppp_backlight_device = bd;

	printk(KERN_ERR "%s initialized\n", __func__);
	return 0;
}

static int sppp_backlight_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sppp_backlight_driver = {
	.probe          = sppp_backlight_probe,
	.remove         = sppp_backlight_remove,
	.driver         = {
		.name   = "sppp_backlight",
	},
};


/*
 * Module init / exit
 */

static int __init sppp_backlight_init(void)
{
	sppp_backlight_client.id = BACKLIGHT;
	sppp_backlight_client.decode = NULL;
	sppp_client_register(&sppp_backlight_client);

	return platform_driver_register(&sppp_backlight_driver);
}

static void __exit sppp_backlight_exit(void)
{
	platform_driver_unregister(&sppp_backlight_driver);
	sppp_client_remove(&sppp_backlight_client);
	return;
}

module_init(sppp_backlight_init);
module_exit(sppp_backlight_exit);


MODULE_AUTHOR("Chris Jenkins <chris.jenkins@genesi-usa.com>");
MODULE_DESCRIPTION("SPPP Backlight Driver");
MODULE_LICENSE("GPL");
