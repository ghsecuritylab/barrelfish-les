/**
 * \file
 * \brief Kernel round-robin scheduling policy
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>
#include <dispatch.h>
#include <kcb.h>
#include <group.h>

#include <timer.h> // update_sched_timer

static struct dcb *schedule_leader_core(void)
{
    struct kcb* kcb_current = GROUP_PER_CORE_KCB_CURRENT;
    // empty ring
    if(kcb_current->dcb_ring == NULL) {
        return NULL;
    }

    assert(kcb_current->dcb_ring->next != NULL);
    assert(kcb_current->dcb_ring->prev != NULL);

    kcb_current->dcb_ring = kcb_current->dcb_ring->next;
    #ifdef CONFIG_ONESHOT_TIMER
    update_sched_timer(kernel_now + kernel_timeslice);
    #endif

    return kcb_current->dcb_ring;
}

static struct dcb *schedule_member_core(void)
{
    static int rate = 0;
    // 二分之一时间去运行新group中的东西
    rate++;
    struct kcb* kcb_current = GROUP_PER_CORE_KCB_CURRENT;
    if (rate % 2 == 0) {
        kcb_current = get_cur_group()->kcb_current;
    }

    // empty ring
    if(kcb_current->dcb_ring == NULL) {
        return NULL;
    }

    assert(kcb_current->dcb_ring->next != NULL);
    assert(kcb_current->dcb_ring->prev != NULL);

    kcb_current->dcb_ring = kcb_current->dcb_ring->next;

    struct dcb* ret = kcb_current->dcb_ring;
    return ret;
}

/**
 * \brief Scheduler policy.
 *
 * \return Next DCB to schedule or NULL if wait for interrupts.
 */
struct dcb *schedule(void)
{
    return is_leader_core() ? schedule_leader_core() : schedule_member_core();
}

void make_runnable(struct dcb *dcb)
{
    struct kcb* kcb_current = GROUP_KCB_RUNNING;
    // Insert into schedule ring if not in there already
    if(dcb->prev == NULL || dcb->next == NULL) {
        assert(dcb->prev == NULL && dcb->next == NULL);

        // Ring empty
        if(kcb_current->dcb_ring == NULL) {
            kcb_current->dcb_ring = dcb;
            dcb->next = dcb;
        }

        // Insert after current ring position
        dcb->prev = kcb_current->dcb_ring;
        dcb->next = kcb_current->dcb_ring->next;
        kcb_current->dcb_ring->next->prev = dcb;
        kcb_current->dcb_ring->next = dcb;
    }
}

/**
 * \brief Remove 'dcb' from scheduler ring.
 *
 * Removes dispatcher 'dcb' from the scheduler ring. If it was not in
 * the ring, this function is a no-op. The postcondition for this
 * function is that dcb is not in the ring.
 *
 * \param dcb   Pointer to DCB to remove.
 */
void scheduler_remove(struct dcb *dcb)
{
    struct kcb* kcb_current = GROUP_KCB_RUNNING;
    // No-op if not in scheduler ring
    if(dcb->prev == NULL || dcb->next == NULL) {
        assert(dcb->prev == NULL && dcb->next == NULL);
        return;
    }

    struct dcb *next = kcb_current->dcb_ring->next;

    // Remove dcb from scheduler ring
    dcb->prev->next = dcb->next;
    dcb->next->prev = dcb->prev;
    dcb->prev = dcb->next = NULL;

    // Removing dcb_ring
    if(dcb == kcb_current->dcb_ring) {
        if(dcb == next) {
            // Only guy in the ring
            kcb_current->dcb_ring = NULL;
        } else {
            // Advance dcb_ring
            kcb_current->dcb_ring = next;
        }
    }
}

/**
 * \brief Yield 'dcb' for the rest of the current timeslice.
 *
 * Re-sorts 'dcb' into the scheduler queue with its release time increased by
 * the timeslice period. It is an error to yield a dispatcher not in the
 * scheduler queue.
 *
 * \param dcb   Pointer to DCB to remove.
 */
void scheduler_yield(struct dcb *dcb)
{
    if(dcb->prev == NULL || dcb->next == NULL) {
        struct dispatcher_shared_generic *dsg =
            get_dispatcher_shared_generic(dcb->disp);
        panic("Yield of %.*s not in scheduler queue", DISP_NAME_LEN,
              dsg->name);
    }

    // No-op for the round-robin scheduler
}

void scheduler_reset_time(void)
{
    // No-Op in RR scheduler
}

void scheduler_convert(void)
{
    struct kcb* kcb_current = GROUP_KCB_RUNNING;
    enum sched_state from = kcb_current->sched;
    switch (from) {
        case SCHED_RBED:
        {
            // initialize RR ring
            struct dcb *last = NULL;
            for (struct dcb *i = kcb_current->queue_head; i; i = i->next)
            {
                i->prev = last;
                last = i;
            }
            // at this point: we have a dll, but need to close the ring
            kcb_current->queue_head->prev = kcb_current->queue_tail;
            kcb_current->queue_tail->next = kcb_current->queue_head;
            break;
        }
        case SCHED_RR:
            // do nothing
            break;
        default:
            printf("don't know how to convert %d to RBED state\n", from);
            break;
    }
    kcb_current->dcb_ring = kcb_current->queue_head;
    for (struct dcb *i = kcb_current->dcb_ring; i != kcb_current->dcb_ring; i=i->next) {
        printf("dcb %p\n  prev=%p\n  next=%p\n", i, i->prev, i->next);
    }
}

void scheduler_restore_state(void)
{
    // No-Op in RR scheduler
}

void schedule_now(struct dcb *dcb)
{
    return;
}
