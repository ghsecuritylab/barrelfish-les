/**
 * \file
 * \brief kernel driver for the spinlock module, used for serial output
 * see Jetson-tk1 TRM chapter 4 for a functional description
 */

/*
 * Copyright (c) 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __JETSONTK1_SPINLOCK_H__
#define __JETSONTK1_SPINLOCK_H__

#include <kernel.h>

#define JETSONTK1_SPINLOCK_NUM_LOCKS 32

/**
 * \brief Map the Jetson-tk1 spinlock device into kernel virtual memory
 * and reset it.
 */
extern errval_t jetsontk1_spinlock_init(void);

/**
 * \brief acquire an Jetson-tk1 spinlock.  We repeatedly read the
 * register; 0 means we have the lock, 1 means we have to try again.
 */
extern void jetsontk1_spinlock_acquire( unsigned locknumber );

/**
 * \brief release an Jetson-tk1 spinlock.
 */
extern void jetsontk1_spinlock_release( unsigned locknumber );

#endif  // _JETSONTK1_SPINLOCK_H__
