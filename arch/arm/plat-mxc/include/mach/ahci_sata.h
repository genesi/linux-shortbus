/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
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

#ifndef __PLAT_MXC_AHCI_SATA_H__
#define __PLAT_MXC_AHCI_SATA_H__

enum {
	HOST_CAP = 0x00,
	HOST_CAP_SSS = (1 << 27), /* Staggered Spin-up */
	HOST_PORTS_IMPL	= 0x0c,
	HOST_TIMER1MS = 0xe0, /* Timer 1-ms */
};

extern int sata_init(struct device *dev, void __iomem *addr);
extern void sata_exit(struct device *dev);
#endif /* __PLAT_MXC_AHCI_SATA_H__ */
