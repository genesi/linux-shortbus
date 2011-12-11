#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/delay.h>

#include <linux/sppp.h>

static struct sppp_client sppp_pwr_client;

/* Callback for incoming data from SPM */
static void sppp_get_data(sppp_rx_t *packet)
{

}

/* Send power suspend sequence to SPM */
static void sppp_pwr_suspend(void)
{

        sppp_tx_t sppp_tx_g;

        sppp_start(&sppp_tx_g, SPPP_PWR_ID);
        sppp_data(&sppp_tx_g, PWR_ID_GOTO_STANDY);
        sppp_stop(&sppp_tx_g);

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

