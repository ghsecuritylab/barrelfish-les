/*
 * Copyright (c) 2009-2013 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <group.h>
#include <kernel.h>
#include <dispatch.h>
#include <arm.h>
#include <platform.h>
/* XXX - not AArch64-compatible. */
#include <cp15.h>
#include <exceptions.h>
#include <exec.h>
#include <kputchar.h>
#include <misc.h>
#include <stdio.h>
#include <wakeup.h>
#include <irq.h>
#include <gic.h>
#include <systime.h>

void handle_user_page_fault(lvaddr_t fault_address,
                            arch_registers_state_t* save_area,
                            struct dispatcher_shared_arm *disp)
{
    // XXX
    // Set DCB_CURRENT->disabled correctly.  This should really be
    // done in exceptions.S
    // XXX
    assert(DCB_CURRENT != NULL);
    assert((struct dispatcher_shared_arm *)(DCB_CURRENT->disp) == disp);
    if (dispatcher_is_disabled_ip((dispatcher_handle_t)disp, save_area->named.pc)) {
        assert(save_area == dispatcher_get_trap_save_area((dispatcher_handle_t)disp));
        DCB_CURRENT_DISABLED = true;
    } else {
        assert(save_area == dispatcher_get_enabled_save_area((dispatcher_handle_t)disp));
        DCB_CURRENT_DISABLED = false;
    }

    lvaddr_t handler;
    uintptr_t saved_pc = save_area->named.pc;

    assert(DCB_CURRENT->disp_cte.cap.type == ObjType_Frame);

    printk(LOG_WARN, "user page fault%s in '%.*s': addr %"PRIxLVADDR
                      " IP %"PRIxPTR"\n",
           DCB_CURRENT_DISABLED ? " WHILE DISABLED" : "", DISP_NAME_LEN,
           disp->d.name, fault_address, saved_pc);

    if (DCB_CURRENT_DISABLED) {
        handler = disp->d.dispatcher_pagefault_disabled;
        DCB_CURRENT->faults_taken++;
    } else {
        handler = disp->d.dispatcher_pagefault;
    }

    if (DCB_CURRENT->faults_taken > 2) {
        printk(LOG_WARN, "handle_user_page_fault: too many faults, "
               "making domain unrunnable\n");
        DCB_CURRENT->faults_taken = 0; // just in case it gets restarted
        scheduler_remove(DCB_CURRENT);
        dispatch(schedule());
    } else {
        //
        // Upcall to dispatcher
        //
        // NB System might be cleaner with a prototype
        // dispatch context that has R0-R3 to be overwritten
        // plus initial stack, thread, and gic registers. Could do
        // a faster resume_for_upcall().
        //
        union registers_arm resume_area;

        resume_area.named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
        resume_area.named.pc   = handler;
        resume_area.named.r0   = disp->d.udisp;
        resume_area.named.r1   = fault_address;
        resume_area.named.r2   = 0;
        resume_area.named.r3   = saved_pc;
        resume_area.named.r9   = disp->got_base;

        // SP is set by handler routine.

        // Upcall user to save area
        dispatcher_set_disabled((dispatcher_handle_t)disp, 1);
        resume(&resume_area);
    }
}

void handle_user_undef(lvaddr_t fault_address,
                       arch_registers_state_t* save_area,
                       struct dispatcher_shared_arm *disp)
{
    // XXX
    // Set DCB_CURRENT->disabled correctly.  This should really be
    // done in exceptions.S
    // XXX
    assert(DCB_CURRENT != NULL);
    assert((struct dispatcher_shared_arm *)(DCB_CURRENT->disp) == disp);
    if (dispatcher_is_disabled_ip((dispatcher_handle_t)disp, save_area->named.pc)) {
        assert(save_area == dispatcher_get_trap_save_area((dispatcher_handle_t)disp));
        DCB_CURRENT_DISABLED = true;
    } else {
        assert(save_area == dispatcher_get_enabled_save_area((dispatcher_handle_t)disp));
        DCB_CURRENT_DISABLED = false;
    }

    union registers_arm resume_area;

    assert(DCB_CURRENT->disp_cte.cap.type == ObjType_Frame);
    printk(LOG_WARN, "user undef fault%s in '%.*s': IP %p\n",
           DCB_CURRENT_DISABLED ? " WHILE DISABLED" : "", DISP_NAME_LEN,
           disp->d.name, fault_address);

    resume_area.named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    resume_area.named.pc   = disp->d.dispatcher_trap;
    resume_area.named.r0   = disp->d.udisp;
    resume_area.named.r1   = ARM_EVECTOR_UNDEF;
    resume_area.named.r2   = 0;
    resume_area.named.r3   = fault_address;
    resume_area.named.r9   = disp->got_base;

    // Upcall user to save area
    dispatcher_set_disabled((dispatcher_handle_t)disp, 1);
    resume(&resume_area);
}

/* XXX - not 64-bit clean, not AArch64-compatible. */
static int32_t bkpt_decode(lvaddr_t fault_address)
{
    int32_t bkpt_id = -1;
    if ((fault_address & 3) == 0 && fault_address >= KERNEL_OFFSET) {
        const uint32_t bkpt_mask = 0xfff000f0;
        const uint32_t bkpt_isn  = 0xe1200070;

        uintptr_t isn = *((uintptr_t*)fault_address);
        if ((isn & bkpt_mask) == bkpt_isn) {
            bkpt_id = (int32_t)((isn & 0xf) | ((isn & 0xfff00) >> 4));
        }
    }
    return bkpt_id;
}

/* XXX - not 64-bit clean, not AArch64-compatible. */
void fatal_kernel_fault(uint32_t evector, lvaddr_t address,
                        arch_registers_state_t* save_area)
{
    int i;

    /* Force the print spinlock release.  We're panicking now anyway, and if
     * the kernel fault was *inside* kprintf, then we'll spin forever here,
     * and never actually report the panic. */
    /* XXX - implement lock_do_i_hold(), so we only do this if we're the
     * holder. */
    kprintf_end();

    printk(LOG_PANIC, "\n");
    printk(LOG_PANIC, "Kernel fault at %08"PRIxLVADDR
                      " vector %08"PRIx32"\n\n", address, evector);
    printk(LOG_PANIC, "Processor save_area at: %p\n", save_area);

    for (i = 0; i < 16; i++) {
        const char *extrainfo = "";

        switch(i) {
        case 13:
            extrainfo = "\t(sp)";
            break;

        case 14:
            extrainfo = "\t(lr)";
            break;

        case 15:
            {
                char str[128];
                snprintf(str, 128, "\t(pc)\t%08"PRIx32,
                         save_area->regs[R0_REG + i] -
                         (uint32_t)&kernel_first_byte +
                         0x100000);
                extrainfo = str;
            }
            break;
        }

        printk(LOG_PANIC, "r%d\t%08"PRIx32"%s\n", i, save_area->regs[R0_REG + i], extrainfo);
    }
    printk(LOG_PANIC, "cpsr\t%08"PRIx32"\n", save_area->regs[CPSR_REG]);
    printk(LOG_PANIC, "called from: #%08"PRIx32"\n",
            (lvaddr_t)__builtin_return_address(0) -
            (lvaddr_t)&kernel_first_byte + 0x100000);

    switch (evector) {
        case ARM_EVECTOR_UNDEF:
            panic("Undefined instruction.\n");
            break;

        case ARM_EVECTOR_PABT: {
            int ifsr = cp15_read_ifsr();
            if (ifsr == 0) {
                int bkpt = bkpt_decode(address);
                if (bkpt >= 0) {
                    panic("Breakpoint: %4x\n", bkpt);
                }
            }
            panic("Prefetch abort: ifsr %08x\n", ifsr);
        }

        case ARM_EVECTOR_DABT: {
            uint32_t dfsr = cp15_read_dfsr();

            printf("\n");

            printf("Data abort ");
            if((dfsr >> 11) & 1) {
                printf("on write\n");
            } else {
                printf("on read\n");
            }

            switch((dfsr & 0xf) | (dfsr & 0x400)) {
            case 1:
                printf("Alignment fault\n");
                break;

            case 4:
                printf("Instruction cache-maintenance fault\n");
                break;

            case 5:
                printf("Translation fault on section\n");
                break;

            case 6:
                printf("Translation fault on page\n");
                break;

            case 8:
                printf("Synchronous external abort\n");
                break;

            default:
                printf("Unknown fault\n");
                break;
            }

            panic("Data abort: dfsr %08"PRIx32"\n", dfsr);
        }

        case ARM_EVECTOR_IRQ: {
            uint32_t irq = gic_get_active_irq();
            panic("IRQ %"PRIu32" in the kernel", irq);
        }

        default:
          panic("Caused by evector: %02"PRIx32, evector);
          break;
    }
}

void handle_fiq_kernel(arch_registers_state_t* save_area, uintptr_t fault_pc)
{
    panic("FIQ Interrupt in the kernel");
}

void handle_fiq(arch_registers_state_t* save_area,
                uintptr_t fault_pc,
                struct dispatcher_shared_generic *disp)
{
    panic("FIQ interrupt from user mode");
}

void handle_irq_kernel(arch_registers_state_t* save_area,
                       uintptr_t fault_pc)
{
    /* In-kernel interrupts are bugs, except if we'd gone to sleep in
     * wait_for_interrupt(), in which case there is no current dispatcher. */
    if(waiting_for_interrupt) {
        waiting_for_interrupt= 0;
    }
    else {
        fatal_kernel_fault(ARM_EVECTOR_IRQ, fault_pc, save_area);
    }

    handle_irq(save_area, fault_pc, NULL);
}

static void set_got_base_lazy(lvaddr_t got_base) {
    lvaddr_t origin;
    __asm__ volatile (
        "mrc p15, 0, %[origin_got], c13, c0, 4\n\t"
        "mcr p15, 0, %[got_base], c13, c0, 4\n\t"
        :[origin_got] "=r" (origin)
        :[got_base] "r" (got_base)
    );
    printk(LOG_ERR, "origin got_base 0x%x, new got_base 0x%x\n", origin, got_base);
}

void handle_irq(arch_registers_state_t* save_area,
                uintptr_t fault_pc,
                struct dispatcher_shared_arm *disp)
{
    // if (my_core_id != cp15_get_cpu_id()) {
    //     printk(LOG_ERR, "Get IRQ after join\n");
    // }
    // XXX
    // Set DCB_CURRENT->disabled correctly.  This should really be
    // done in exceptions.S
    // XXX
    if(DCB_CURRENT != NULL) {
        assert((struct dispatcher_shared_arm *)(DCB_CURRENT->disp) == disp);
        if (dispatcher_is_disabled_ip((dispatcher_handle_t)disp, fault_pc)) {
            assert(save_area ==
                   dispatcher_get_disabled_save_area(
                       (dispatcher_handle_t)disp));
            DCB_CURRENT_DISABLED = true;
        } else {
            assert(save_area ==
                   dispatcher_get_enabled_save_area(
                       (dispatcher_handle_t)disp));
            DCB_CURRENT_DISABLED = false;
        }
    }

    // Retrieve the current IRQ number
    uint32_t irq = 0;
    irq = gic_get_active_irq();
    debug(SUBSYS_DISPATCH, "IRQ %"PRIu32" while %s\n", irq,
          DCB_CURRENT_DISABLED ? "disabled": "enabled" );

    struct group* group = get_group(cp15_get_cpu_id());
    assert(group);
    if (group->detach[cp15_get_cpu_id()]) {
        printk(LOG_WARN, "Fuck the group\n");
        set_got_base_lazy(group->got_base);
        group->detach[cp15_get_cpu_id()] = false;
        printk(LOG_WARN, "Fuck the group\n");
        printk(LOG_WARN, "Detach complete, core: %d\n", (int)cp15_get_cpu_id());
        timer_interrupt(irq);
        dispatch(NULL);
    }

    // Offer it to the timer
    if (timer_interrupt(irq)) {
        // Timer interrupt, timer_interrupt() acks it at the timer.
        wakeup_check(systime_now());
        dispatch(schedule());
    }
    // this is the (still) unacknowledged startup interrupt sent by the BSP
    // we just acknowledge it here
    else if(irq == 1)
    {
        gic_ack_irq(irq);
        dispatch(schedule());
    }
    else {
        gic_ack_irq(irq);
        send_user_interrupt(irq);
        panic("Unhandled IRQ %"PRIu32"\n", irq);
    }
}
