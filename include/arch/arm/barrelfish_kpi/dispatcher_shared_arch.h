/**
 * \file
 * \brief Architecture specific dispatcher struct shared between kernel and user
 */

/*
 * Copyright (c) 2010, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef ARCH_ARM_BARRELFISH_KPI_DISPATCHER_SHARED_ARCH_H
#define ARCH_ARM_BARRELFISH_KPI_DISPATCHER_SHARED_ARCH_H

#include <target/arm/barrelfish_kpi/dispatcher_shared_target.h>
#include <barrelfish/invocations.h>

static inline coreid_t get_core_id(void) 
{
#ifndef IN_KERNEL
    struct capref task_cap_kernel;
    task_cap_kernel.cnode = cnode_task;
    task_cap_kernel.slot = TASKCN_SLOT_KERNELCAP;
    coreid_t my_core_id = -1;
    // struct sysret ret = cap_invoke1(task_cap_kernel, KernelCmd_Get_core_id);
    // if (err_is_fail(ret.error)) {
        dispatcher_handle_t handle = curdispatcher();
        struct dispatcher_shared_generic *disp_gen = get_dispatcher_shared_generic(handle);
        my_core_id = disp_gen->curr_core_id;
         //my_core_id = 0;
    // }
    if (disp_gen->spanned) {
        struct sysret ret = cap_invoke1(task_cap_kernel, KernelCmd_Get_core_id);
        if (err_is_ok(ret.error)) {
            my_core_id = ret.value;
        }
    }
    return (coreid_t)my_core_id;
#else
	uint8_t cpu_id;
	__asm volatile(
			"mrc 	p15, 0, %[cpu_id], c0, c0, 5\n\t" // get the MPIDR register
			"and	%[cpu_id], %[cpu_id], #0xF\n\t"
			:[cpu_id] "=r" (cpu_id)
		);
    // assert(cpu_id == 0);
	return (coreid_t)cpu_id;
#endif
}

/**
 * \brief Returns whether dispatcher is currently disabled, given IP.
 *
 * \param disp  Pointer to dispatcher
 * \param ip    User-level instruction pointer.
 *
 * \return true if dispatcher disabled, false otherwise.
 */
static inline bool dispatcher_is_disabled_ip(dispatcher_handle_t handle,
                                             uintptr_t rip)
{
    struct dispatcher_shared_generic *disp =
        get_dispatcher_shared_generic(handle);
    /* one crit_pc pair */
    struct dispatcher_shared_arm *disparm =
        get_dispatcher_shared_arm(handle);
    return disp->disabled_all[get_core_id()] ||
        (disparm->crit_pc_low <= rip && rip < disparm->crit_pc_high);
}


static inline uint32_t dispatcher_get_disabled_by_coreid(dispatcher_handle_t handle, coreid_t coreid)
{
    return ((struct dispatcher_shared_generic *)handle)->disabled_all[coreid] != 0 ? 1 : 0;
}

static inline void dispatcher_set_disabled_by_coreid(dispatcher_handle_t handle, coreid_t coreid, uint32_t disabled)
{
    disabled = (disabled != 0);
    ((struct dispatcher_shared_generic *)handle)->disabled_all[coreid] = disabled;
}

static inline void dispatcher_try_set_disabled_by_coreid(dispatcher_handle_t handle, coreid_t coreid, uint32_t disabled, bool* was_enabled)
{
    // if(get_core_id() != 0)printf("origin %d\n", ((struct dispatcher_shared_generic *)handle)->disabled_all);
    struct dispatcher_shared_generic *disp_gen = get_dispatcher_shared_generic(handle);
    *was_enabled = !__atomic_test_and_set(disp_gen->disabled_all + coreid, __ATOMIC_SEQ_CST);
    // if(get_core_id() != 0)printf("new %d\n", ((struct dispatcher_shared_generic *)handle)->disabled_all);
}

static inline uint32_t dispatcher_get_disabled(dispatcher_handle_t handle) 
{
    return dispatcher_get_disabled_by_coreid(handle, get_core_id());
}

static inline void dispatcher_set_disabled(dispatcher_handle_t handle, uint32_t disabled)
{
    return dispatcher_set_disabled_by_coreid(handle, get_core_id(), disabled);
}

static inline void dispatcher_try_set_disabled(dispatcher_handle_t handle, uint32_t disabled, bool* was_enabled)
{
    return dispatcher_try_set_disabled_by_coreid(handle, get_core_id(), disabled, was_enabled);
}

static inline arch_registers_state_t*
dispatcher_get_enabled_save_area_by_coreid(dispatcher_handle_t handle, coreid_t coreid)
{
    // assert(coreid == 0);
    return &((struct dispatcher_shared_arm *)handle)->areas[coreid].enabled_save_area;
    // return &((struct dispatcher_shared_arm *)handle)->enabled_save_area;
}

static inline arch_registers_state_t*
dispatcher_get_disabled_save_area_by_coreid(dispatcher_handle_t handle, coreid_t coreid)
{
    // assert(coreid == 0);
    return &((struct dispatcher_shared_arm *)handle)->areas[coreid].disabled_save_area;
    // return &((struct dispatcher_shared_arm *)handle)->disabled_save_area;
}

static inline arch_registers_state_t*
dispatcher_get_trap_save_area_by_coreid(dispatcher_handle_t handle, coreid_t coreid)
{
    // assert(coreid == 0);
    return &((struct dispatcher_shared_arm *)handle)->areas[coreid].trap_save_area;
    // return &((struct dispatcher_shared_arm *)handle)->trap_save_area;
}

static inline arch_registers_state_t*
dispatcher_get_enabled_save_area(dispatcher_handle_t handle)
{
    return dispatcher_get_enabled_save_area_by_coreid(handle, get_core_id());
}

static inline arch_registers_state_t*
dispatcher_get_disabled_save_area(dispatcher_handle_t handle)
{
    return dispatcher_get_disabled_save_area_by_coreid(handle, get_core_id());
}

static inline arch_registers_state_t*
dispatcher_get_trap_save_area(dispatcher_handle_t handle)
{
    return dispatcher_get_trap_save_area_by_coreid(handle, get_core_id());
}

static inline arch_registers_state_t*
dispatcher_get_enabled_save_area_shared(dispatcher_handle_t handle)
{
    return &((struct dispatcher_shared_arm *)handle)->enabled_save_area;
}

static inline arch_registers_state_t*
dispatcher_get_disabled_save_area_shared(dispatcher_handle_t handle)
{
    return &((struct dispatcher_shared_arm *)handle)->disabled_save_area;
}

static inline arch_registers_state_t*
dispatcher_get_trap_save_area_shared(dispatcher_handle_t handle)
{
    return &((struct dispatcher_shared_arm *)handle)->trap_save_area;
}
#endif // ARCH_ARM_BARRELFISH_KPI_DISPATCHER_SHARED_ARCH_H
