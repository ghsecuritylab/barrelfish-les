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

#define MAX_CORE 16

static inline coreid_t get_core_id(void)
{
    uint32_t ret = 0;
    __asm("mrc p15, 0, %[ret], c13, c0, 3" : [ret] "=r" (ret));
    ret &= 0xfff;
    return (coreid_t)ret;
}

#endif
