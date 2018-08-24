/**
 * \file
 * \brief Platform code for the Cortex-A15 processors on NVIDIA JETSON-TK1 Board.
 */

/*
 * Copyright (c) 2009-2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>

#include <maps/jetsontk1_map.h>

/* RAM starts at 2G (2 ** 31) on the Jetson-tk1 */
/* XXX - MMAP */
lpaddr_t phys_memory_start= GEN_ADDR(31);

#define NUM_UARTS 4
unsigned int serial_console_port = 3;
unsigned int serial_debug_port = 3;
unsigned int serial_num_physical_ports = NUM_UARTS;

const lpaddr_t uart_base[NUM_UARTS] = {
    JETSON_APB_UARTA,
    JETSON_APB_UARTB,
    JETSON_APB_UARTC,
    JETSON_APB_UARTD
};

const size_t uart_size[NUM_UARTS] = {
    JETSON_APB_UARTA_SIZE,
    JETSON_APB_UARTB_SIZE,
    JETSON_APB_UARTC_SIZE,
    JETSON_APB_UARTD_SIZE,
};
