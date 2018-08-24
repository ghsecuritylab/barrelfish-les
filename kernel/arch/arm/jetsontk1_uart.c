/**
 * \file
 * \brief Kernel serial driver for the JESTON-TK1 UARTs.
 */

/*
 * Copyright (c) 2012-2015, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <jetsontk1_uart.h>
#include <dev/jetsontk1/jetson_uart_dev.h>
#include <kernel.h>
#include <arm.h>
#include <paging_kernel_arch.h>
#include <platform.h>
#include <serial.h>

//
// Serial console and debugger interfaces
//
#define NUM_PORTS 4
static jetson_uart_t ports[NUM_PORTS];

static void jetson_uart_hw_init(jetson_uart_t *uart);

#define MSG(port, format, ...) \
    printk( LOG_NOTE, "Jetson-tk1 serial[%d]: "format, port, ## __VA_ARGS__ )

#define CONFING_SYS_NS16550_CLK			408000000
#define CONFING_SYS_NS16550_BAUDRATE	115200

static int
calc_divisor(void)
{
	const unsigned int mode_x_div = 16;

	return (CONFING_SYS_NS16550_CLK / mode_x_div
			/ CONFING_SYS_NS16550_BAUDRATE );
}

/*
 * Initialize UARTs before the MMU is on.
 */
errval_t serial_early_init(unsigned port)
{
    assert(!paging_mmu_enabled());
    assert(port < serial_num_physical_ports);
    jetson_uart_initialize(&ports[port], (mackerel_addr_t)uart_base[port]);
    jetson_uart_hw_init(&ports[port]);
    return SYS_ERR_OK;
}

/*
 * Re-initialize UARTs after the MMU is on.
 */
void jetson_uart_init(unsigned port, lvaddr_t base, bool initialize_hw)
{
    assert(paging_mmu_enabled());
    assert(port < serial_num_physical_ports);
    jetson_uart_initialize(&ports[port], (mackerel_addr_t)base);
    if (initialize_hw) jetson_uart_hw_init(&ports[port]);
}

/*
 * Initialise Jetson-tk1 UART hardware
 * UART TRM 23.3
 */
static void jetson_uart_hw_init(jetson_uart_t *uart)
{
	int baud_divisor;
	baud_divisor = calc_divisor();

    while(!jetson_uart_LSR_tsr_e_rdf(uart));

    jetson_uart_LCR_dlab_wrf(uart, 1);

    jetson_uart_DLL_clock_lsb_wrf(uart, 0);
    jetson_uart_DLM_clock_msb_wrf(uart, 0);

    jetson_uart_LCR_dlab_wrf(uart, 0);

    jetson_uart_MCR_out2_wrf(uart, 1);
    jetson_uart_MCR_dtr_wrf(uart, 1);
    jetson_uart_MCR_rts_wrf(uart, 1);

    jetson_uart_FCR_t fcr = jetson_uart_FCR_default;

    fcr = jetson_uart_FCR_rx_fifo_trig_insert(fcr, 1);
    fcr = jetson_uart_FCR_tx_fifo_trig_insert(fcr, 1);
    fcr = jetson_uart_FCR_dma_mode_insert(fcr, 0); // disable DMA
    fcr = jetson_uart_FCR_fifo_en_insert(fcr, 0); // disable FIFOs
    jetson_uart_FCR_wr(uart, fcr);


    jetson_uart_FCR_tx_fifo_clear_wrf(uart, 1);
    jetson_uart_FCR_rx_fifo_clear_wrf(uart, 1);


    //5 clear IER register
    jetson_uart_IER_wr(uart, 0);
    //6 config mode B
    jetson_uart_LCR_dlab_wrf(uart, 1);
    //7 new divisor value --> 115200 baud == 0x00, 0x1A (dlh, dll)
    jetson_uart_DLL_clock_lsb_wrf(uart, baud_divisor & 0xff);
    jetson_uart_DLM_clock_msb_wrf(uart, (baud_divisor >> 8) & 0xff);
    //8 register operational mode
    jetson_uart_LCR_dlab_wrf(uart, 0);

    //set up interrupt
    jetson_uart_IER_rhr_it_wrf(uart, 1);
    jetson_uart_IER_modem_sts_it_wrf(uart, 1);
    //9 load irq config --> only rhr irq for now

    jetson_uart_LCR_t lcr = jetson_uart_LCR_default;
    lcr = jetson_uart_LCR_parity_en_insert(lcr, 0);       // No parity
    lcr = jetson_uart_LCR_nb_stop_insert(lcr, 0);         // 1 stop bit
    lcr = jetson_uart_LCR_char_length_insert(lcr, jetson_uart_wl_8bits); // 8 data bits
    jetson_uart_LCR_wr(uart, lcr);

	//for DEBUG
	#if 0
	int i;
	for(i = 0; i < 7; i++){
    	while(!jetson_uart_LSR_thr_e_rdf(uart));
    	jetson_uart_THR_thr_wrf(uart, 'c');
	}
	while(!jetson_uart_LSR_thr_e_rdf(uart));
	jetson_uart_THR_thr_wrf(uart, '\r');
	while(!jetson_uart_LSR_thr_e_rdf(uart));
	jetson_uart_THR_thr_wrf(uart, '\n');
	#endif
}

/**
 * \brief Prints a single character to a serial port.
 */
void serial_putchar(unsigned port, char c)
{
    assert(port <= NUM_PORTS);
    jetson_uart_t *uart = &ports[port];

    // Wait until FIFO can hold more characters
    while(!jetson_uart_LSR_thr_e_rdf(uart));

    // Write character
    jetson_uart_THR_thr_wrf(uart, c);
}

/**
 * \brief Reads a single character from the default serial port.
 */
char serial_getchar(unsigned port)
{
    assert(port <= NUM_PORTS);
    jetson_uart_t *uart = &ports[port];

    /* Read until the interrupt is deasserted. */
    char c = '\0';
	//while(!jetson_uart_LSR_rx_data_r_rdf(uart));
    while(jetson_uart_IIR_it_pending_rdf(uart) == 0) {
        c = jetson_uart_RBR_rbr_rdf(uart);
    }
	//Return the character
    return c;
}
