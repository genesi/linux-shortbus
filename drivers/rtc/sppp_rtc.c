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

uint32_t rawtime = 0;

/* Callback for incoming data from SPM */
static void sppp_get_data(sppp_rx_t *packet)
{
        uint32_t time;

        time =   packet->input[0]<<24;
        time |= (packet->input[1]<<16);
        time |= (packet->input[2]<< 8);
        time |= (packet->input[3]<< 0);

        rawtime = time;
}

/* Send read sequence to SPM and get data when it arrives */
static int sppp_rtc_readtime(struct device *dev, struct rtc_time *tm)
{

        sppp_tx_t sppp_tx_g;

        sppp_start(&sppp_tx_g, SPPP_RTC_ID);
        sppp_data(&sppp_tx_g, RTC_ID_GET);
        sppp_stop(&sppp_tx_g);

        while (rawtime == 0);

        rtc_time_to_tm(rawtime, tm);

        rawtime = 0;

        return rtc_valid_tm(tm);
}

static int sppp_rtc_settime(struct device *dev, struct rtc_time *tm)
{

        sppp_tx_t sppp_tx_g;
        unsigned long now;

        rtc_tm_to_time(tm, &now);

        sppp_start(&sppp_tx_g, SPPP_RTC_ID);
        sppp_data(&sppp_tx_g, RTC_ID_SET);
        sppp_data(&sppp_tx_g, (now&0xFF000000)>>24);
        sppp_data(&sppp_tx_g, (now&0x00FF0000)>>16);
        sppp_data(&sppp_tx_g, (now&0x0000FF00)>> 8);
        sppp_data(&sppp_tx_g, (now&0x000000FF)>> 0);
        sppp_stop(&sppp_tx_g);

        return 0;
}

static struct rtc_class_ops sppp_rtc_ops = {
        .read_time        = sppp_rtc_readtime,
        .set_time         = sppp_rtc_settime,
};

static int __init sppp_rtc_probe(struct platform_device *pdev)
{

        struct rtc_sppp *rtc;

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

        sppp_rtc_client.id = RTC;
        sppp_rtc_client.decode = sppp_get_data;
        sppp_client_register(&sppp_rtc_client);

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
        .remove			= __exit_p(sppp_rtc_remove),
        .driver         = {
                .name   = "sppp_rtc",
                .owner  = THIS_MODULE,
        },
};

static struct platform_device sppp_rtc = {
        .name = "sppp_rtc",
        .id = -1,
};

static int __init sppp_rtc_init(void)
{

        platform_device_register(&sppp_rtc);
        return platform_driver_probe(&sppp_rtc_driver, sppp_rtc_probe);
}

static void __exit sppp_rtc_exit(void)
{
        sppp_client_remove(&sppp_rtc_client);
        platform_driver_unregister(&sppp_rtc_driver);
        platform_device_unregister(&sppp_rtc);
}

module_init(sppp_rtc_init);
module_exit(sppp_rtc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPPP RTC client");
MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");

