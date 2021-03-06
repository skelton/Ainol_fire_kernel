/*
 * arch/arm/mach-meson2/include/mach/am_regs.h
 *
 * Copyright (C) 2010 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __MACH_MESSON2_AM_REGS_H
#define __MACH_MESSON2_AM_REGS_H

#ifndef __ASSEMBLY__

#include <asm/io.h>

#define WRITE_CBUS_REG(reg, val) __raw_writel(val, CBUS_REG_ADDR(reg))
#define READ_CBUS_REG(reg) (__raw_readl(CBUS_REG_ADDR(reg)))
#define WRITE_CBUS_REG_BITS(reg, val, start, len) \
    WRITE_CBUS_REG(reg,	(READ_CBUS_REG(reg) & ~(((1L<<(len))-1)<<(start)) )| ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define READ_CBUS_REG_BITS(reg, start, len) \
    ((READ_CBUS_REG(reg) >> (start)) & ((1L<<(len))-1))
#define CLEAR_CBUS_REG_MASK(reg, mask) WRITE_CBUS_REG(reg, (READ_CBUS_REG(reg)&(~(mask))))
#define SET_CBUS_REG_MASK(reg, mask)   WRITE_CBUS_REG(reg, (READ_CBUS_REG(reg)|(mask)))

#define WRITE_AXI_REG(reg, val) __raw_writel(val, AXI_REG_ADDR(reg))
#define READ_AXI_REG(reg) (__raw_readl(AXI_REG_ADDR(reg)))
#define WRITE_AXI_REG_BITS(reg, val, start, len) \
    WRITE_AXI_REG(reg,	(READ_AXI_REG(reg) & ~(((1L<<(len))-1)<<(start)) )| ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define READ_AXI_REG_BITS(reg, start, len) \
    ((READ_AXI_REG(reg) >> (start)) & ((1L<<(len))-1))
#define CLEAR_AXI_REG_MASK(reg, mask) WRITE_AXI_REG(reg, (READ_AXI_REG(reg)&(~(mask))))
#define SET_AXI_REG_MASK(reg, mask)   WRITE_AXI_REG(reg, (READ_AXI_REG(reg)|(mask)))

#define WRITE_AHB_REG(reg, val) __raw_writel(val, AHB_REG_ADDR(reg))
#define READ_AHB_REG(reg) (__raw_readl(AHB_REG_ADDR(reg)))
#define WRITE_AHB_REG_BITS(reg, val, start, len) \
    WRITE_AHB_REG(reg,	(READ_AHB_REG(reg) & ~(((1L<<(len))-1)<<(start)) )| ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define READ_AHB_REG_BITS(reg, start, len) \
    ((READ_AHB_REG(reg) >> (start)) & ((1L<<(len))-1))
#define CLEAR_AHB_REG_MASK(reg, mask) WRITE_AHB_REG(reg, (READ_AHB_REG(reg)&(~(mask))))
#define SET_AHB_REG_MASK(reg, mask)   WRITE_AHB_REG(reg, (READ_AHB_REG(reg)|(mask)))

#define WRITE_APB_REG(reg, val) __raw_writel(val, APB_REG_ADDR(reg))
#define READ_APB_REG(reg) (__raw_readl(APB_REG_ADDR(reg)))
#define WRITE_APB_REG_BITS(reg, val, start, len) \
    WRITE_APB_REG(reg,	(READ_APB_REG(reg) & ~(((1L<<(len))-1)<<(start)) )| ((unsigned)((val)&((1L<<(len))-1)) << (start)))
#define READ_APB_REG_BITS(reg, start, len) \
    ((READ_APB_REG(reg) >> (start)) & ((1L<<(len))-1))
#define CLEAR_APB_REG_MASK(reg, mask) WRITE_APB_REG(reg, (READ_APB_REG(reg)&(~(mask))))
#define SET_APB_REG_MASK(reg, mask)   WRITE_APB_REG(reg, (READ_APB_REG(reg)|(mask)))

/* for back compatible alias */
#define WRITE_MPEG_REG(reg, val) \
    WRITE_CBUS_REG(reg, val)
#define READ_MPEG_REG(reg) \
    READ_CBUS_REG(reg)
#define WRITE_MPEG_REG_BITS(reg, val, start, len) \
    WRITE_CBUS_REG_BITS(reg, val, start, len)
#define READ_MPEG_REG_BITS(reg, start, len) \
    READ_CBUS_REG_BITS(reg, start, len)
#define CLEAR_MPEG_REG_MASK(reg, mask) \
    CLEAR_CBUS_REG_MASK(reg, mask)
#define SET_MPEG_REG_MASK(reg, mask) \
    SET_CBUS_REG_MASK(reg, mask)
#endif

#ifdef CONFIG_VMSPLIT_3G
#define IO_ETH_BASE                 0xf9010000
#define IO_USB_A_BASE               0xf9040000
#define IO_USB_B_BASE               0xf90C0000
#define IO_WIFI_BASE                0xf9300000
#define IO_SATA_BASE                0xf9400000

#define IO_CBUS_PHY_BASE            0xc1100000 // 2M
#define IO_AXI_BUS_PHY_BASE         0xc1300000 // 1M
#define IO_PL310_PHY_BASE           0xc4200000 // 4K
#define IO_AHB_BUS_PHY_BASE         0xc9000000 // 16M
#define IO_APB_BUS_PHY_BASE         0xd0040000 // 512K

#define IO_CBUS_BASE                0xf1100000
#define IO_AXI_BUS_BASE             0xf1300000
#define IO_PL310_BASE               0xf4200000
#define IO_AHB_BUS_BASE             0xf9000000
#define IO_APB_BUS_BASE             0xfa040000

#define MESON_PERIPHS1_VIRT_BASE    0xf1108400
#define MESON_PERIPHS1_PHYS_BASE    0xc1108400
#endif

#ifdef CONFIG_VMSPLIT_2G
#define IO_ETH_BASE                 0xc9010000
#define IO_USB_A_BASE               0xC9040000
#define IO_USB_B_BASE               0xC90C0000
#define IO_WIFI_BASE                0xC9300000
#define IO_SATA_BASE                0xC9400000

#define IO_CBUS_PHY_BASE            0xc1100000
#define IO_AXI_BUS_PHY_BASE         0xc1300000
#define IO_PL310_PHY_BASE           0xc4200000
#define IO_AHB_BUS_PHY_BASE         0xc9000000
#define IO_APB_BUS_PHY_BASE         0xd0040000

#define IO_CBUS_BASE                0xc1100000
#define IO_AXI_BUS_BASE             0xc1300000
#define IO_PL310_BASE               0xc4200000
#define IO_AHB_BUS_BASE             0xc9000000
#define IO_APB_BUS_BASE             0xd0040000

#define MESON_PERIPHS1_VIRT_BASE    0xc1108400
#define MESON_PERIPHS1_PHYS_BASE    0xc1108400
#endif

#ifdef CONFIG_VMSPLIT_1G
#error Unsupported Memory Split Type
#endif

#define CBUS_REG_OFFSET(reg) ((reg) << 2)
#define CBUS_REG_ADDR(reg)	 (IO_CBUS_BASE + CBUS_REG_OFFSET(reg))

#define AXI_REG_OFFSET(reg)  ((reg) << 2)
#define AXI_REG_ADDR(reg)	 (IO_AXI_BUS_BASE + AXI_REG_OFFSET(reg))

#define AHB_REG_OFFSET(reg)  ((reg) << 2)
#define AHB_REG_ADDR(reg)	 (IO_AHB_BUS_BASE + AHB_REG_OFFSET(reg))

#define APB_REG_OFFSET(reg)  (reg)
#define APB_REG_ADDR(reg)	 (IO_APB_BUS_BASE + APB_REG_OFFSET(reg))
#define APB_REG_ADDR_VALID(reg) (((unsigned long)(reg) & 3) == 0)

#include "c_stb_define.h"
#include <mach/regs.h>
#include "pctl.h"
#include "dmc.h"
#include "am_eth_reg.h"


#endif /* __MACH_MESSON2_AM_REGS_H */
