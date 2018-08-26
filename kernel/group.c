#include <group.h>
#include <kernel.h>
#include <startup.h>
#include <stdio.h>
#include <cp15.h>
volatile int kernel_lock;

// struct group_mgmt* global_group_mgmt __attribute__((section(".text")));

static void group_init_globals(void)
{
    // __asm volatile (
    //     ".global global_group_mgmt             ""\n\t"
    //     ".type global_group_mgmt, STT_OBJECT   ""\n\t"
    //     "global_group_mgmt:                    ""\n\t"
    //     "  .word 0                             ""\n\t"
    // );

    kernel_lock = 0;
    global_group_mgmt = (struct group_mgmt*)bsp_alloc_phys(sizeof(struct group_mgmt));
    printf("MGMT: %lx\n", global_group_mgmt);
}

static lvaddr_t get_got_base(void)
{
    lvaddr_t origin;
    __asm__ volatile (
        "mrc p15, 0, %[origin_got], c13, c0, 4\n\t"
        :[origin_got] "=r" (origin)
    );
    return origin;
}

static lvaddr_t set_got_base_lazy(lvaddr_t got_base) {
    lvaddr_t origin = get_got_base();
    __asm__ volatile (
        "mcr p15, 0, %[got_base], c13, c0, 4\n\t"
        :[origin_got] "=r" (origin)
        :[got_base] "r" (got_base)
    );
    return origin;
}

struct group* get_group(coreid_t coreid)
{
    return &global_group_mgmt->groups[coreid];
}

static struct group* set_cur_group_by_coreid(coreid_t coreid, struct group* g)
{
    // here should be more state maintain
    // 比如说当前group都包含了哪些core
    return global_group_mgmt->cur_group[coreid] = g;
}

struct group* get_cur_group_by_coreid(coreid_t coreid)
{
    return global_group_mgmt->cur_group[coreid];
}

struct group* get_cur_group(void)
{
    coreid_t id = get_core_id();
    if (global_group_mgmt->lazy_load_target_group[id]) {
        set_cur_group_by_coreid(id, global_group_mgmt->lazy_load_target_group[id]);
        global_group_mgmt->lazy_load_target_group[id] = NULL;
    }
    return get_cur_group_by_coreid(id);
}

void set_cur_group_lazy(struct group* g)
{
    global_group_mgmt->lazy_load_target_group[get_core_id()] = g;
    // XXX this should be moved to arch specified code
    set_got_base_lazy(g->got_base);
}

static void group_init_common(void)
{
    coreid_t coreid = get_core_id();
    struct group* cur = set_cur_group_by_coreid(coreid, get_group(coreid));
    cur->enabled = true;
    cur->got_base = set_got_base_lazy(get_got_base());
    cur->group_id = coreid;
    cur->per_core_state[coreid].enabled = true;

    printf("MGMT: %lx\n", global_group_mgmt);
}

void group_bsp_init(void)
{
    group_init_globals();
    group_init_common();
}

void group_app_init(void)
{
    group_init_common();
}