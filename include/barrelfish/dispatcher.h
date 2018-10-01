/**
 * \file
 * \brief Generic dispatcher structure private to the user
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef BARRELFISH_DISPATCHER_H
#define BARRELFISH_DISPATCHER_H

#include <barrelfish/dispatch.h>
#include <barrelfish/core_state_arch.h>
#include <barrelfish/heap.h>
#include <barrelfish/threads.h>

struct lmp_chan;
struct ump_chan;
struct deferred_event;
struct notificator;

/// Maximum number of buffered capability receive slots
#define MAX_RECV_SLOTS   4

struct dispatcher_generic_per_core_state {
    /// Currently-running (or last-run) thread, if any
    struct thread *current;

    /// Thread run queue (all threads eligible to be run in this core)
    struct thread *runnable_threads;
};

// Architecture generic user only dispatcher struct
struct dispatcher_generic {
    /// stack for traps and disabled pagefaults
    uintptr_t trap_stack[DISPATCHER_STACK_WORDS * MAX_CORE] __attribute__ ((aligned (16)));
    /// all other dispatcher upcalls run on this stack
    uintptr_t stack[DISPATCHER_STACK_WORDS * MAX_CORE] __attribute__ ((aligned (16)));

    /// Cap to this dispatcher, used for creating new endpoints
    struct capref dcb_cap;

    /// Thread run queue (all threads waiting for attach core)
    struct thread *runq;

    struct dispatcher_generic_per_core_state dispatcher_per_core_state[MAX_CORE];

#ifdef CONFIG_INTERCONNECT_DRIVER_LMP
    /// List of LMP endpoints to poll
    struct lmp_endpoint *lmp_poll_list;

    /// List of LMP channels waiting to retry a send
    struct lmp_chan *lmp_send_events_list;
    struct ump_chan *ump_send_events_list;

    /// LMP endpoint heap state
    struct heap lmp_endpoint_heap;
#endif // CONFIG_INTERCONNECT_DRIVER_LMP

    /// Queue of deferred events (i.e. timers)
    struct deferred_event *deferred_events;

    /// The core the dispatcher is running on
    coreid_t core_id;

    uintptr_t timeslice;

    /// Per core dispatcher state
    struct core_state_arch core_state;

    /// Tracing buffer
    struct trace_buffer *trace_buf;

    struct thread *cleanupthread;
    struct thread_mutex cleanupthread_lock;

    /// Last FPU-using thread
    struct thread *fpu_thread;

    /// Domain ID cache
    domainid_t domain_id;

    /// virtual address of the eh_frame
    lvaddr_t eh_frame;

    /// size of the eh frame
    size_t   eh_frame_size;

    /// virtual address of the eh_frame
    lvaddr_t eh_frame_hdr;

    /// size of the eh frame
    size_t   eh_frame_hdr_size;

    /// list of polled channels
    struct waitset_chanstate *polled_channels;
    
    struct notificator *notificators;

    struct capref recv_slots[MAX_RECV_SLOTS];///< Queued cap recv slots
    int8_t recv_slot_count;                 ///< number of currently queued recv slots

    spinlock_t disp_lock;
};

static inline struct thread** get_current_thread(struct dispatcher_generic *handler, coreid_t coreid) 
{
    return &(handler->dispatcher_per_core_state[coreid].current);
}

static inline void lock_disp(dispatcher_handle_t handle)
{
    struct dispatcher_generic *disp_gen = (struct dispatcher_generic*)handle;
    spinlock_acquire(&disp_gen->disp_lock);
}

static inline void unlock_disp(dispatcher_handle_t handle)
{
    struct dispatcher_generic *disp_gen = (struct dispatcher_generic*)handle;
    spinlock_release(&disp_gen->disp_lock);
}

#define CURRENT_THREAD_OF_DISP_CORE(h, c) (*get_current_thread(h, c))
#define CURRENT_THREAD_OF_DISP(h) (*get_current_thread(h, get_core_id()))
#define CURRENT_THREAD (*get_current_thread(get_dispatcher_generic(curdispatcher()), get_core_id()))

#endif // BARRELFISH_DISPATCHER_H
