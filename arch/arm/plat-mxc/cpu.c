
#include <linux/module.h>
#include <mach/hardware.h>

unsigned int __mxc_cpu_type;
EXPORT_SYMBOL(__mxc_cpu_type);

void mxc_set_cpu_type(unsigned int type)
{
	__mxc_cpu_type = type;
}

void imx_print_silicon_rev(const char *cpu, int srev)
{
	if (srev == IMX_CHIP_REVISION_UNKNOWN)
		pr_info("CPU identified as %s, unknown revision\n", cpu);
	else
		pr_info("CPU identified as %s, silicon rev %d.%d\n",
				cpu, (srev >> 4) & 0xf, srev & 0xf);
}

int mxc_jtag_enabled;		/* OFF: 0 (default), ON: 1 */
int uart_at_24; 			/* OFF: 0 (default); ON: 1 */
/*
 * Here are the JTAG options from the command line. By default JTAG
 * is OFF which means JTAG is not connected and WFI is enabled
 *
 *       "on" --  JTAG is connected, so WFI is disabled
 *       "off" -- JTAG is disconnected, so WFI is enabled
 */

static int __init jtag_wfi_setup(char *p)
{
	if (memcmp(p, "on", 2) == 0) {
		mxc_jtag_enabled = 1;
		p += 2;
	} else if (memcmp(p, "off", 3) == 0) {
		mxc_jtag_enabled = 0;
		p += 3;
	}
	return 0;
}
early_param("jtag", jtag_wfi_setup);
