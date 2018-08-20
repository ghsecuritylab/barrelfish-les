/**
 * \file
 * \brief kernel driver for the spinlock module, used for serial output
 * see Jetson-tk1 TRM.4.0 for a functional description
 */

/*
 * Copyright (c) 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>
#include <paging_kernel_arch.h>
#include <jetsontk1_spinlock.h>
#include <dev/jetsontk1/jetson_semaphore_dev.h>
#include <maps/jetsontk1_map.h>


#define MSG(format, ...) printk( LOG_NOTE, "Jetson-tk1 spinlock: "format, ## __VA_ARGS__ )

static jetson_semaphore_t spinlock;
static bool locks_initialized;  //allows early initialization

/**
 * \brief Map the Jetson-tk1 spinlock device into kernel virtual memory
 * and reset it.
 */

errval_t jetsontk1_spinlock_init(void)
{
    if (locks_initialized) {
	MSG("already initialized; skipping.\n");
	return SYS_ERR_OK;
    }

    lvaddr_t base = paging_map_device(JETSON_ARBI_SEMAPHORE_BASE,
				      JETSON_ARBI_SEMAPHORE_SIZE);
    MSG("init base = 0x%"PRIxLVADDR"\n", base);
    jetson_spinlock_initialize(&spinlock, (mackerel_addr_t)base);

    // This next line to rest the module is probably not needed.
    //jetson_spinlock_sysconfig_softreset_wrf(&spinlock, 1);
    //
    //MSG("testing spinlock: first read (should be 0): 0x%x\n",
    //            jetson_spinlock_lock_reg_i_taken_rdf(&spinlock, 2));
    //
    //MSG("testing spinlock: second read (should be 1): 0x%x\n",
    //            jetson_spinlock_lock_reg_i_taken_rdf(&spinlock, 2));
    //
    //jetson_spinlock_lock_reg_i_taken_wrf(&spinlock, 2, 0);//clear lock
    locks_initialized = true;
    MSG("init done.\n");
    return SYS_ERR_OK;
}

/**
 * \brief acquire an OMAP44xx spinlock.  We repeatedly read the
 * register; 0 means we have the lock, 1 means we have to try again.
 */
void jetsontk1_spinlock_acquire( unsigned locknumber ) {
    assert( locknumber < JETSONTK1_SPINLOCK_NUM_LOCKS );
    while( jetson_semaphore_sema_gnt_st_rd(&spinlock) & locknumber)
		;
}

/**
 * \brief release an OMAP44xx spinlock.
 */
void jetsontk1_spinlock_release( unsigned locknumber ) {
    assert( locknumber < JETSONTK1_SPINLOCK_NUM_LOCKS );
    jetson_semaphore_sema_put_wr(&spinlock, locknumber); //clears lock
}

