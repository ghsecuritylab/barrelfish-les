#pragma once
#include <stdbool.h>
#include <barrelfish_kpi/types.h>
#include <barrelfish_kpi/cpu_arch.h>
#include <barrelfish_kpi/spinlocks_arch.h>

#ifndef MAX_CORE
#define MAX_CORE 4
#endif

struct group_per_core_state {
    bool enabled;
    /// Current execution dispatcher (when in system call or exception)
    struct dcb* dcb_current;
};

struct dcb_per_core_state {
    struct group* group;
    bool disabled;
    bool scheduled; // ever scheduled in this core
    bool need_flush_tlb;
};

struct group {
    bool enabled;
    groupid_t group_id;
    lvaddr_t got_base;
    size_t core_count;
    bool core_mask[MAX_CORE];
    struct group_per_core_state per_core_state[MAX_CORE];
    volatile int* lock;
    struct kcb* kcb_current;
};

struct group_mgmt {
    struct group groups[MAX_CORE];      // 本机上所有的Group
    struct group* cur_group[MAX_CORE];  // 每个Core当前所属的Group

    struct group* lazy_load_target_group[MAX_CORE];
    int can_update[MAX_CORE]; //表明是否可以用lazy_load的值更新cur_group，在每次重新由用户态进入时则可以更新
};

extern struct group_mgmt *global_group_mgmt;

void group_bsp_init(void);
void group_app_init(void);
struct group* get_group(coreid_t coreid);

struct group* get_cur_group_by_coreid(coreid_t coreid);
struct group* get_cur_group(void);
void set_cur_group_lazy(struct group* g);

static inline struct dcb** get_dcb_current(void)
{
    return &get_cur_group()->per_core_state[get_core_id()].dcb_current;
}

struct kcb;
struct kcb** get_kcb_current(void);

static inline bool is_leader_core(void)
{
    return get_core_id() == get_cur_group()->group_id;
}

static inline void lock_cur_group(void)
{
    acquire_spinlock((spinlock_t*)get_cur_group()->lock);
}

static inline void unlock_cur_group(void)
{
    release_spinlock((spinlock_t*)get_cur_group()->lock);
}

#define GROUP_PER_CORE_DCB_CURRENT (*get_dcb_current())
#define GROUP_PER_CORE_DCB_CURRENT_DISABLED (GROUP_PER_CORE_DCB_CURRENT->per_core_state[get_core_id()].disabled)

///< The kernel control block
#define GROUP_PER_CORE_KCB_CURRENT (*get_kcb_current())

void print_r0(void* ptr);