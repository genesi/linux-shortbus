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
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sppp.h>

#define STATUS_POWER_STATE_START_BIT    (8)
#define STATUS_POWER_STATE_SHIFT(val)   ((val)<<STATUS_POWER_STATE_START_BIT)
#define STATUS_POWER_STATE_MASK         STATUS_POWER_STATE_SHIFT(0x3)
#define STATUS_POWER_STATE_COLD_RESET   STATUS_POWER_STATE_SHIFT(1)
#define STATUS_POWER_STATE_WARM_RESET   STATUS_POWER_STATE_SHIFT(2)
#define STATUS_POWER_STATE_OFF          STATUS_POWER_STATE_SHIFT(3)

static struct sppp_client sppp_pwr_client;

/* Callback for incoming data from SPM */
static void sppp_get_data(sppp_rx_t *packet)
{
}

/* Send power suspend sequence to SPM */
static void sppp_pwr_suspend(void)
{
	uint32_t fw_status;

	sppp_tx_t sppp_tx;

	fw_status = STATUS_POWER_STATE_OFF;
	sppp_start(&sppp_tx, SPPP_STATUS_ID);
	sppp_data(&sppp_tx, STATUS_ID_SET);
	sppp_data(&sppp_tx, (fw_status>>24) );
	sppp_data(&sppp_tx, (fw_status>>16) );
	sppp_data(&sppp_tx, (fw_status>> 8) );
	sppp_data(&sppp_tx, (fw_status>> 0) );
	sppp_stop(&sppp_tx);
}

static int __init sppp_pwr_init(void)
{
	sppp_pwr_client.id = POWER;
	sppp_pwr_client.decode = sppp_get_data;
	sppp_client_register(&sppp_pwr_client);

	pm_power_off = sppp_pwr_suspend;

	return 0;
}

static void __exit sppp_pwr_exit(void)
{
	sppp_client_remove(&sppp_pwr_client);
	pm_power_off = NULL;
}

module_init(sppp_pwr_init);
module_exit(sppp_pwr_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPPP Power Management client");
MODULE_AUTHOR("Johan Dams <jdmm@genesi-usa.com>");

