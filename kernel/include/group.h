#pragma once
#include <stdbool.h>
#include <barrelfish_kpi/types.h>

#define MAX_CORE 16

typedef int groupid_t;

struct group_per_core_state {
    bool enabled;
    struct dcb* dcb_current;
    struct kcb* kcb_current;
};

struct dcb_per_core_state {
    struct group* group;
    bool disabled_arr[MAX_CORE];
};

struct group {
    bool enabled;
    groupid_t group_id;
    lvaddr_t got_base;
    struct group_per_core_state per_core_state[MAX_CORE];
};

struct group_mgmt {
    struct group groups[MAX_CORE];      // 本机上所有的Group
    struct group* cur_group[MAX_CORE];  // 每个Core当前所属的Group
};

extern struct group_mgmt *global_group_mgmt;
extern volatile int kernel_lock;

void group_bsp_init(void);
void group_app_init(void);
struct group* get_group(coreid_t coreid);

struct group* get_cur_group_by_coreid(coreid_t coreid);
struct group* set_cur_group_by_coreid(coreid_t coreid, struct group* g);
struct group* get_cur_group(void);

coreid_t get_real_cpu_id(void);

static inline struct dcb** get_dcb_current(void)
{
    return &get_cur_group()->per_core_state[get_real_cpu_id()].dcb_current;
}

#define GROUP_PER_CORE_DCB_CURRENT (*get_dcb_current())
#define GROUP_PER_CORE_DCB_CURRENT_DISABLED (GROUP_PER_CORE_DCB_CURRENT->disabled_arr[get_real_cpu_id()])
