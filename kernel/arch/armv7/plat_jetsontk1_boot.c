/**
 * \file
 * \brief ROM-controlled core boot for the NVIDIA Tegra K1
 */

/*
 * Copyright (c) 2009-2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>

#include <boot_protocol.h>
#include <cp15.h>
#include <dev/jetsontk1/jetson_exception_vectors_dev.h>
#include <dev/jetsontk1/jetson_flow_controller_dev.h>
#include <maps/jetsontk1_map.h>

#define  JETSON_FLOW_CTRL_CPU_HALT_VALUE	0x48000000
/* The boot driver entry point. */
extern char start;

/* Wake the second core, so that it advances into the same wfe loop as all
 * other ARM systems. */
void
plat_advance_aps(void) {
    jetson_exception_vectors_t vectors;
	jetson_flow_controller_t fctrl;
	//init device address base
    jetson_exception_vectors_initialize(&vectors,
            (mackerel_addr_t)JETSON_EXCEPTION_VECTORS_BASE);
	jetson_flow_controller_initialize(&fctrl,
			(mackerel_addr_t)JETSON_FLOW_CONTROLLER_BASE);

	/*
	 * wake up other cores
	 */
    //setup boot driver entry for new cores
    jetson_exception_vectors_reset_vector_wr(&vectors, (uint32_t)&start);

	__asm__ volatile ("SEV");
	//reset cores: core 1, 2, 3
	jetson_flow_controller_FCHCPU1C0_wr(&fctrl, 0x1);
	//dsb();
	//jetson_flow_controller_FCHCPU1E0_rd(&fctrl);
	jetson_flow_controller_FCHCPU2C0_wr(&fctrl, 0x1);
	jetson_flow_controller_FCHCPU3C0_wr(&fctrl, 0x1);

	jetson_flow_controller_FCHCPU1E0_wr(&fctrl, JETSON_FLOW_CTRL_CPU_HALT_VALUE);
	//dsb();
	//jetson_flow_controller_FCHCPU1E0_rd(&fctrl);
	jetson_flow_controller_FCHCPU2E0_wr(&fctrl, JETSON_FLOW_CTRL_CPU_HALT_VALUE);
	jetson_flow_controller_FCHCPU3E0_wr(&fctrl, JETSON_FLOW_CTRL_CPU_HALT_VALUE);

}
