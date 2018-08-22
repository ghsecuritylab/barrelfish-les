#ifndef KERNEL_GROUP_H
#define KERNEL_GROUP_H
#include "kernel.h"
#include <barrelfish_kpi/spinlocks_arch.h>

typedef uint32_t groupid_t;

struct scheduler
{
};

struct kcb_list
{
    struct kcb *kcb;
    struct kcb_list *next, *prev;
};

struct group
{
    groupid_t id;
    coreid_t origin_core;
    // group tree
    struct group *children, *sibling, *parent;
    uint32_t member_count;

    struct kcb_list kcb_list_head;
    spinlock_t lock;

    lvaddr_t got_base;

    bool detach[16];
};

static inline void set_got_base(lpaddr_t got_base)
{
    __asm__ volatile (
        "ldr "__XSTRING(PIC_REGISTER)", %[got_base]"
        ::[got_base] "m" (got_base)
    );
}

static inline lpaddr_t get_got_base(void)
{
    lpaddr_t got_base;
    __asm__ volatile (
        "str "__XSTRING(PIC_REGISTER)", %[got_base]"
        :[got_base] "=m" (got_base)
    );
    return got_base;
}

extern struct group kernel_group;
extern struct group *current_group;
extern volatile lvaddr_t kernel_lock;

void init_group(void);
void combineTo(struct group *target_group);
struct group* get_group(coreid_t coreid);
#endif
