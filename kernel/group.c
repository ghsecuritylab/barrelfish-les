/**
 * \file
 * \author SunSijie
 * \brief Core group support
 */

#include "group.h"
#include <barrelfish_kpi/spinlocks_arch.h>
#include <kcb.h>

extern struct group* group_save_zone[];

struct group kernel_group;
struct group *current_group;

spinlock_t group_lock;

extern lvaddr_t test_var1;
extern lvaddr_t test_var2;
extern lvaddr_t test_var3;

void init_group(void) {
    kernel_group = (struct group) {
        .origin_core = my_core_id,
        .parent = NULL,
        .children = NULL,
        .sibling = NULL,
        .member_count = 0,
        .got_base = get_got_base(),
        .detach = (bool[]){false, },
        .kcb_list_head = (struct kcb_list) {
            .kcb = kcb_current,
            .next = NULL,
            .prev = NULL,
        },
    };
    for (int i = 0; i < 16; i++)
    {
      kernel_group.detach[i] = false;
    }
    current_group = &kernel_group;
    group_save_zone[my_core_id] = current_group;
    printk(LOG_ERR, "kernel lock address is 0x%x", &kernel_lock);
    printk(LOG_ERR, "my_core_id address is 0x%x", &my_core_id);

    printk(LOG_ERR, "kernel test_var1 address is 0x%x, value is %d\n", &test_var1, test_var1);
    printk(LOG_ERR, "kernel test_var2 address is 0x%x, value is %d\n", &test_var2, test_var2);
    printk(LOG_ERR, "kernel test_var3 address is 0x%x, value is %d\n", &test_var3, test_var3++);
}

void combineTo(struct group *target_group) {
    acquire_spinlock(&target_group->lock);
    current_group->kcb_list_head.next = &(target_group->kcb_list_head);
    target_group->kcb_list_head.prev = &(current_group->kcb_list_head);
    release_spinlock(&target_group->lock);
}

struct group* get_group(coreid_t core_id) {
    return group_save_zone[core_id];
} 
