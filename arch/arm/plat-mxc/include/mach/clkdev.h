#ifndef __ASM_MACH_CLKDEV_H
#define __ASM_MACH_CLKDEV_H

#ifndef CONFIG_USE_COMMON_STRUCT_CLK
#define __clk_get(clk) ({ 1; })
#define __clk_put(clk) do { } while (0)
#endif

#endif
