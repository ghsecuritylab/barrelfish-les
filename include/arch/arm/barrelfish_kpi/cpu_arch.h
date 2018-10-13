/**
 * \file
 * \brief Arch specific CPU declarations
 */

/*
 * Copyright (c) 2010, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef ARCH_ARM_BARRELFISH_KPI_CPU_H
#define ARCH_ARM_BARRELFISH_KPI_CPU_H

/// This CPU supports lazy FPU context switching?
#undef FPU_LAZY_CONTEXT_SWITCH

#include <barrelfish_kpi/types.h>
#include <bitmacros.h>

#define MAX_CORE 8

static inline coreid_t get_core_id(void)
{
#ifndef IN_KERNEL
    uint32_t ret = 0;
    __asm volatile ("mrc p15, 0, %[ret], c13, c0, 3" : [ret] "=r" (ret));
    ret &= MASK(10);
    return (coreid_t)ret;
#else
    volatile uint8_t cpu_id;
    __asm volatile(
                    "mrc    p15, 0, %[cpu_id], c0, c0, 5\n\t" // get the MPIDR register
                    "and    %[cpu_id], %[cpu_id], #0xF\n\t"
                    :[cpu_id] "=r" (cpu_id)
            );

    return cpu_id;
#endif
}

#endif
