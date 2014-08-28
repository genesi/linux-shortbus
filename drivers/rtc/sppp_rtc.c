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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sppp.h>

static struct sppp_client sppp_rtc_client;

struct rtc_sppp {
	struct rtc_device       *rtc;
};

uint32_t rawtime;

/* Callback for incoming data from STM */
static void sppp_get_data(const sppp_rx_t *packet)
{
	const uint8_t rtc_resp_type = packet->input[0];

	switch (rtc_resp_type) {
	case RTC_ID_SET:
		printk("RTC_ID_SET response not handled\n");
		break;
	case RTC_ID_GET: {
		uint32_t time = 0;

		if (packet->pos != 5) {
			printk(KERN_ERR "RTC_ID_GET invalid response!\n");
			return;
		}

		time |= (packet->input[1] << 24);
		time |= (packet->input[2] << 16);
		time |= (packet->input[3] <<  8);
		time |= (packet->input[4] <<  0);

		printk(KERN_ERR "RTC time is %u\n", time);
		rawtime = time;
		break;
	}
	case RTC_ID_GET_ALARM:
		printk(KERN_ERR "RTC_ID_GET_ALARM not handled!\n");
		break;
	default:
		printk(KERN_ERR "SPPP_RTC_ID response type not recognized: %d\n",
		       rtc_resp_type);
		break;
	}
}

/* Send read sequence to STM */
static inline void sppp_request_gettime(void)
{
	sppp_client_send_start(RTC, SPPP_RTC_ID);
	sppp_client_send_data(RTC, RTC_ID_GET);
	sppp_client_send_stop(RTC);
}

/* Send write sequence to STM */
static inline void sppp_do_settime(unsigned long now)
{
	sppp_client_send_start(RTC, SPPP_RTC_ID);
	sppp_client_send_data(RTC, RTC_ID_SET);
	sppp_client_send_data(RTC, (now&0xFF000000) >> 24);
	sppp_client_send_data(RTC, (now&0x00FF0000) >> 16);
	sppp_client_send_data(RTC, (now&0x0000FF00) >>  8);
	sppp_client_send_data(RTC, (now&0x000000FF) >>  0);
	sppp_client_send_stop(RTC);
}

/** Send read sequence to STM and get data when it arrives, unless time is cached
 *  global variable @rawtime must be 0 when this function exits
 */
static int sppp_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	/* sppp_tx_t sppp_tx_g; */
	int tries = 30;

	// check if we cached the RTC time
	if (rawtime != 0) {
		goto set_sys_time;
	}

	// get the raw time from the STM
	sppp_request_gettime();
	for (; tries > 0 && rawtime == 0; --tries) {
		msleep_interruptible(1);
	}

	if (rawtime == 0) {
		printk(KERN_ERR "%s failed to get system time after wait\n",
		       __func__);
		return -1;
	}

set_sys_time:
	rtc_time_to_tm(rawtime, tm);
	rawtime = 0;

	return rtc_valid_tm(tm);
}

static int sppp_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	/* sppp_tx_t sppp_tx_g; */
	unsigned long now;
	
	printk(KERN_ERR "%s\n", __func__);

	rtc_tm_to_time(tm, &now);
	sppp_do_settime(now);

	return 0;
}

static struct rtc_class_ops sppp_rtc_ops = {
	.read_time        = sppp_rtc_readtime,
	.set_time         = sppp_rtc_settime,
};

static int __init sppp_rtc_probe(struct platform_device *pdev)
{
	struct rtc_sppp *rtc;

	printk(KERN_ERR "%s\n", __func__);

	rtc = kzalloc(sizeof(struct rtc_sppp), GFP_KERNEL);
	if (!rtc) {
		dev_dbg(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, rtc);

	rtc->rtc = rtc_device_register(pdev->name, &pdev->dev,
					&sppp_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc->rtc)) {
		dev_dbg(&pdev->dev, "could not register rtc device\n");
		return PTR_ERR(rtc->rtc);
	}

	dev_info(&pdev->dev, "SPPP RTC for Genesi EfikaSB\n");

	return 0;
}


static int __exit sppp_rtc_remove(struct platform_device *pdev)
{
	struct rtc_sppp *rtc = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver sppp_rtc_driver = {
	.probe		= sppp_rtc_probe,
	.remove			= __exit_p(sppp_rtc_remove),
	.driver         = {
		.name   = "sppp_rtc",
		.owner  = THIS_MODULE,
	},
};

static int __init sppp_rtc_init(void)
{
	int status;

	sppp_rtc_client.id = RTC;
	sppp_rtc_client.decode = sppp_get_data;
	sppp_client_register(&sppp_rtc_client);

	status = platform_driver_register(&sppp_rtc_driver);
	printk(KERN_ERR "%s registered: %d\n", __func__, status);

	if (status == 0) {
		// preempt get_time
		sppp_request_gettime();
	}

	return status;
}

static void __exit sppp_rtc_exit(void)
{
	platform_driver_unregister(&sppp_rtc_driver);
	sppp_client_remove(&sppp_rtc_client);
}

module_init(sppp_rtc_init);
module_exit(sppp_rtc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPPP RTC client");
MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");

