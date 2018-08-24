/**
 * \file
 * \brief Platform code for the NVIDIA Jetson-tk1 SoCs
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>

#include <assert.h>
#include <cp15.h>
#include <errors/errno.h>
#include <gic.h>
#include <global.h>
#include <init.h>
#include <paging_kernel_arch.h>
#include <platform.h>
#include <serial.h>
#include <maps/jetsontk1_map.h>
#include <jetsontk1_uart.h>

#define MSG(format, ...) printk( LOG_NOTE, "Jetosn-tk1: "format, ## __VA_ARGS__ )

/*****************************************************************************
 *
 * Implementation of serial.h
 *
 *****************************************************************************/

/*
 * Initialize the serial ports
 */
errval_t
serial_init(unsigned port, bool initialize_hw) {
    assert(paging_mmu_enabled());
    assert(port < serial_num_physical_ports);

    lvaddr_t base = paging_map_device(uart_base[port], uart_size[port]);
    jetson_uart_init(port, base, initialize_hw);
#if 0
	lvaddr_t test_base = base;
	while (test_base != 0){
		serial_putchar(port, test_base % 16 + '0');
		test_base /= 16;
	}
	serial_putchar(port, 'Y');
	serial_putchar(port, '\r');
	serial_putchar(port, '\n');
#endif
    return SYS_ERR_OK;
};

/* Print system identification. MMU is NOT yet enabled. */
void
platform_print_id(void) {
    printf("This is a Jetson-tk1.\n");
	return ;
}

void
platform_get_info(struct platform_info *pi) {
    pi->arch     = PI_ARCH_ARMV7A;
    pi->platform = PI_PLATFORM_JETSONTK1;
    armv7_get_info(&pi->arch_info.armv7);
}

/* The Jetson-tk1 has 2GB of RAM beginning at address 0x80000000. */
size_t
platform_get_ram_size(void) {
    return (JETSONTK1_DDR_MEM_HIGHADDR - JETSONTK1_DDR_MEM_BASEADDR) + 1;
}
