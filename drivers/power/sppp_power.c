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
#include <linux/power_supply.h>
#include <linux/sppp.h>

/* SPPP Definitions */
#define STATUS_POWER_STATE_START_BIT    (8)
#define STATUS_POWER_STATE_SHIFT(val)   ((val)<<STATUS_POWER_STATE_START_BIT)
#define STATUS_POWER_STATE_MASK         STATUS_POWER_STATE_SHIFT(0x3)
#define STATUS_POWER_STATE_COLD_RESET   STATUS_POWER_STATE_SHIFT(1)
#define STATUS_POWER_STATE_WARM_RESET   STATUS_POWER_STATE_SHIFT(2)
#define STATUS_POWER_STATE_OFF          STATUS_POWER_STATE_SHIFT(3)

static struct sppp_client sppp_pwr_client;

/* Device info */
enum {
	VOLTAGE_WARN_LOW_mV = 6100,
	VOLTAGE_MIN_mV      = 6550,
	VOLTAGE_FUNC_BRK_mV = 7277,
	VOLTAGE_MAX_mV      = 8500,
	VOLTAGE_WARN_HIGH_mV= 8600,
};

enum {
	CHARGE_AVERAGE_mAh = 4000,
};

#define milli_to_micro(x) ((x) * 1000)

struct sppp_pwr_device_info {
	struct device *dev;

	int16_t voltage_raw;	/* units of mV */
	int32_t voltage_uV;	/* units of µV */

	int rem_capacity;	/* percentage */
	int present;		/* presence */
	int charge_status;	/* POWER_SUPPLY_STATUS */

	struct power_supply bat;
	struct device *parent_dev;
	/* work structures for monitoring battery state */
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
};

/* local cache of device-relevant STM information */
enum SPPP_CACHE_UPDATE_STATUS {
	VBAT_UPDATED = 0x01,
	STAT_UPDATED = 0x02,
};

struct sppp_pwr_cache_info {
	uint8_t vbat[2];
	uint8_t stat[4];
	uint8_t was_updated;
};
static struct sppp_pwr_cache_info sppp_cache;

static inline int
SPPP_STATUS_HAS_DCIN(struct sppp_pwr_cache_info *sppp_cache)
{
	return (sppp_cache->stat[0] & 0x80) != 0;
}

static inline int
SPPP_STATUS_IS_CHARGER_ON(struct sppp_pwr_cache_info *sppp_cache)
{
	return (sppp_cache->stat[0] & 0x40) != 0;
}

static inline int
SPPP_STATUS_IS_FW_CHARGE_ON(struct sppp_pwr_cache_info *sppp_cache)
{
	return (sppp_cache->stat[0] & 0x20) != 0;
}

static inline void sppp_pwr_cache_clear(struct sppp_pwr_cache_info *sppp_cache)
{
	memset((void *)sppp_cache, 0, sizeof(struct sppp_pwr_cache_info));
}

static inline void sppp_pwr_cache_vbat_update(const uint8_t *vbat)
{
	/* printk(KERN_ERR "%s VBAT data %x %x", __func__, vbat[0], vbat[1]); */

 	memcpy(sppp_cache.vbat, vbat, sizeof sppp_cache.vbat);

	sppp_cache.was_updated |= VBAT_UPDATED;
}

static inline void sppp_pwr_cache_stat_update(const sppp_rx_t *resp)
{
	if (resp->num < 5) {
		printk(KERN_ERR "%s: invalid response from STM!", __func__);
		return;
	}

	/* printk(KERN_ERR "%s STATUS data: type %d; %x %x %x %x", */
	/*        __func__, resp->input[0] == STATUS_ID_GET, */
	/*        resp->input[1], resp->input[2], resp->input[3], resp->input[4]); */

	memcpy(sppp_cache.stat, &resp->input[1], sizeof sppp_cache.stat);
	sppp_cache.was_updated |= STAT_UPDATED;
}

/*************************************************************
 * STM SPPP Functions - naming conventions                   *
 *     'get'          - SPPP callback function               *
 *     'request'      - Send message expecting a response    *
 *     'do'           - Send message expecting no response   *
 *************************************************************/

/* Callback for incoming data from STM */
static void sppp_get_data(const sppp_rx_t *packet)
{
	switch (packet->id) {
	case SPPP_VBAT_ID:
		sppp_pwr_cache_vbat_update(packet->input);
		break;
	case SPPP_STATUS_ID:
		sppp_pwr_cache_stat_update(packet);
		break;
	}
}

/* Send power suspend sequence to STM */
static void sppp_do_suspend(void)
{
	uint32_t fw_status;

	sppp_tx_t sppp_tx;

	sppp_start(&sppp_tx, SPPP_BKL_ID);
	sppp_data(&sppp_tx, 0);
	sppp_stop(&sppp_tx);

	fw_status = STATUS_POWER_STATE_OFF;
	sppp_start(&sppp_tx, SPPP_STATUS_ID);
	sppp_data(&sppp_tx, STATUS_ID_SET);
	sppp_data(&sppp_tx, (fw_status>>24) );
	sppp_data(&sppp_tx, (fw_status>>16) );
	sppp_data(&sppp_tx, (fw_status>> 8) );
	sppp_data(&sppp_tx, (fw_status>> 0) );
	sppp_stop(&sppp_tx);
}

static void sppp_request_vbat(void) 
{
	sppp_client_send_start(POWER, SPPP_VBAT_ID);
	sppp_client_send_stop(POWER);
}

static void sppp_request_status(void)
{
	sppp_client_status_listen(POWER);

	sppp_client_send_start(POWER, SPPP_STATUS_ID);
	sppp_client_send_data(POWER, STATUS_ID_GET);
	sppp_client_send_stop(POWER);
}

/*
 * Voltage -> Capacity -> Energy calculations
 */
static inline int64_t __P1(int16_t x)
{
	const int64_t xLL = (int64_t)x;

	return (-21467LL * xLL * xLL) + (280764500LL * xLL) - 738197057465LL;

	/* return (-0.00021467 * x2 + 2.8076 * x1 - 7382.0); */
}

static inline int64_t __P2(int16_t x)
{
	int64_t xLL = (int64_t)x;

	return (107638LL * xLL * xLL) + (-1848984615LL * xLL) + 7931615778350LL;

	/* return (0.0010764 * (x) * (x) - 18.49 * (x) + 79316.0); */
}

static inline int __NORMALIZE(int64_t y)
{
	const long capacity_max = 175649784; /* __P1(6550) / 1024 */
	const long capacity_min = -4802448;  /* __P2(8400) / 1024 */

	const long capacity_range = capacity_max - capacity_min;
	const long capacity_diff  = capacity_max - ((long)(y >> 10));

	return min(max(capacity_diff / (capacity_range / 100L), 0L), 100L);

//	return 100.0 - (max(min(100.0 * (8513.0 - y) / (8513.0 - 6257.0), 100.0), 0.0));
}

static int calculate_capacity_percentage(int16_t mv)
{
	if (mv <= VOLTAGE_WARN_LOW_mV) {
		return 0;
	} else if (mv <= VOLTAGE_MIN_mV) {
		return __NORMALIZE(__P1(VOLTAGE_MIN_mV));
	} else if (mv <= VOLTAGE_FUNC_BRK_mV) {
		return __NORMALIZE(__P1(mv));
	} else if (mv <= VOLTAGE_WARN_HIGH_mV) {
		if (__P1(VOLTAGE_FUNC_BRK_mV) < __P2(mv))
			return __NORMALIZE(__P1(VOLTAGE_FUNC_BRK_mV));
		else
			return __NORMALIZE(__P2(mv));
	} else {
		printk(KERN_WARNING "battery voltage exceeds safe operation!");
		return 0;
	}
}

/*
 * Linux kernel power_supply API
 */

#define sppp_pwr_suspend NULL   /* TODO */
#define sppp_pwr_resume  NULL	/* TODO */

static void
sppp_pwr_update_status_charge(struct sppp_pwr_device_info *di)
{
	if (!SPPP_STATUS_HAS_DCIN(&sppp_cache))
		di->charge_status =
			POWER_SUPPLY_STATUS_DISCHARGING;

	else if (SPPP_STATUS_IS_CHARGER_ON(&sppp_cache) &&
	         SPPP_STATUS_IS_FW_CHARGE_ON(&sppp_cache)) {
		if (di->rem_capacity == 100)
			di->charge_status = 
				POWER_SUPPLY_STATUS_FULL;

		else
			di->charge_status = 
				POWER_SUPPLY_STATUS_CHARGING;
	}

	else
		di->charge_status = POWER_SUPPLY_STATUS_UNKNOWN;
}

static void
sppp_pwr_update_status_voltage(struct sppp_pwr_device_info *di)
{
	/* update voltage in mV */
	di->voltage_raw  = ((int16_t)sppp_cache.vbat[0]) << 8;
	di->voltage_raw |= sppp_cache.vbat[1];

	/* update voltage in uV */
	di->voltage_uV = milli_to_micro(di->voltage_raw);

	di->rem_capacity = calculate_capacity_percentage(di->voltage_raw);

	/* TODO checking for garbage voltage to determine battery
	   presence is an ineffective workaround */
	if (di->voltage_raw < 4000) {
		di->present = 0;
		return;
	} else {
		di->present = 1;
		if (di->voltage_raw < VOLTAGE_WARN_LOW_mV)
			printk(KERN_WARNING
			       "battery lifetime at risk at low voltage\n");
	}
}

/* 
 * Update device info with local copy of cached info
 */
static void sppp_pwr_update_status(struct sppp_pwr_device_info *di)
{
	int old_charge_status = di->charge_status;
	int old_presence      = di->present;

	if (!sppp_cache.was_updated)
		return; /* Nothing to do */

	/* NOTE status code has to be executed first to fake the 
	 *      voltage reading for when battery is charging
	 */
	if (sppp_cache.was_updated & STAT_UPDATED)
		sppp_pwr_update_status_voltage(di);

	if (sppp_cache.was_updated & VBAT_UPDATED)
		sppp_pwr_update_status_charge(di);

	/* Trigger battery change event */
	if (di->charge_status != old_charge_status ||
	    di->present       != old_presence)
		power_supply_changed(&di->bat);

	sppp_pwr_cache_clear(&sppp_cache);
}

static void sppp_pwr_monitor_work(struct work_struct *work)
{
	struct sppp_pwr_device_info *di = container_of(work,
		struct sppp_pwr_device_info, monitor_work.work);

	/* TODO make interval longer after data collected */
	const int interval = HZ * 2;

	sppp_request_vbat();
	sppp_request_status();

        sppp_pwr_update_status(di);

	queue_delayed_work(di->monitor_wqueue, &di->monitor_work, interval);
}

/* helper boilerplate for unpacking device info from power supply */
#define to_sppp_pwr_device_info(x) \
	container_of((x), struct sppp_pwr_device_info, bat);

/* Unsupported power_supply API functions */
#define sppp_pwr_external_power_changed NULL
#define sppp_pwr_set_charged NULL

static int sppp_pwr_get_property(struct power_supply *psy,
                                 enum power_supply_property psp,
                                 union power_supply_propval *val)
{
	struct sppp_pwr_device_info *di = to_sppp_pwr_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->charge_status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->present;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = milli_to_micro(VOLTAGE_MAX_mV);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = milli_to_micro(VOLTAGE_MIN_mV);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->voltage_uV;
		break;

	/* NOTE This data is used only to get upower
	        to show 'best guess' percentages */
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		/* units of uWh = mAh * mV */
		val->intval = CHARGE_AVERAGE_mAh *
			(VOLTAGE_MAX_mV - VOLTAGE_MIN_mV);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* units of uWh = mAh * mV */
		val->intval = CHARGE_AVERAGE_mAh *
			((VOLTAGE_MAX_mV - VOLTAGE_MIN_mV) *
			 di->rem_capacity) / 100;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->rem_capacity;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		/* TODO implement */
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sppp_pwr_set_property(struct power_supply *psy,
                                 enum power_supply_property psp,
                                 const union power_supply_propval *val)
{
	switch (psp) {
	default:
		return -EPERM;
	}

	return 0;
}

static int sppp_pwr_property_is_writeable(struct power_supply *psy,
                                          enum power_supply_property psp)
{
	switch (psp) {
	default:
		break;
	}

	return 0;
}

/* TODO fill array of props stub */
static enum power_supply_property sppp_pwr_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

/* TODO fill function stubs */
static int sppp_pwr_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct sppp_pwr_device_info *di;

	printk(KERN_INFO "%s", __func__);

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		retval = -ENOMEM;
		goto di_alloc_failed;
	}

	platform_set_drvdata(pdev, di);

	di->dev			= &pdev->dev;
	di->parent_dev		= pdev->dev.parent;
	di->bat.name		= dev_name(&pdev->dev);
	di->bat.type		= POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties	= sppp_pwr_props;
	di->bat.num_properties	= ARRAY_SIZE(sppp_pwr_props);
	di->bat.get_property	= sppp_pwr_get_property;
	di->bat.set_property	= sppp_pwr_set_property;
	di->bat.property_is_writeable =
	                          sppp_pwr_property_is_writeable;
	di->bat.set_charged	= sppp_pwr_set_charged;
	di->bat.external_power_changed = 
	                          sppp_pwr_external_power_changed;

	di->charge_status = POWER_SUPPLY_STATUS_UNKNOWN;
	di->present = 0;

	retval = power_supply_register(&pdev->dev, &di->bat);
	if (retval) {
		retval = -ESRCH;
		printk(KERN_ERR "\tFailed to register: %d", retval);
		goto register_failed;
	}

	/* initialize and fire off delayed work */
	INIT_DELAYED_WORK(&di->monitor_work, sppp_pwr_monitor_work);
	di->monitor_wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!di->monitor_wqueue) {
		retval = -ESRCH;
		goto workqueue_failed;
	}
	queue_delayed_work(di->monitor_wqueue, &di->monitor_work, HZ * 1);

	return 0;

register_failed:
	{} /* fallthrough */
workqueue_failed:
        power_supply_unregister(&di->bat);
di_alloc_failed:
	kfree(di);
	return retval;
}

static int sppp_pwr_remove(struct platform_device *pdev)
{
	struct sppp_pwr_device_info *di = platform_get_drvdata(pdev);

	printk(KERN_INFO "%s", __func__);

	cancel_delayed_work_sync(&di->monitor_work);
	destroy_workqueue(di->monitor_wqueue);
	power_supply_unregister(&di->bat);
	kfree(di);

	return 0;
}

static struct platform_driver sppp_pwr_driver = {
	.driver = {
		.name  = "sppp_power",
		.owner = THIS_MODULE,
	},
	.probe		= sppp_pwr_probe,
	.remove		= sppp_pwr_remove,
	.suspend	= sppp_pwr_suspend,
	.resume		= sppp_pwr_resume,
};

static int __init sppp_pwr_init(void)
{
	int status;

	sppp_pwr_client.id = POWER;
	sppp_pwr_client.decode = sppp_get_data;
	sppp_client_register(&sppp_pwr_client);

	/* sppp_do_iden(); */

	pm_power_off = sppp_do_suspend;
	
	status = platform_driver_register(&sppp_pwr_driver);
	printk(KERN_INFO "\tregister platform driver: %d", status);
	return status;
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
MODULE_ALIAS("platform:sppp_power");
