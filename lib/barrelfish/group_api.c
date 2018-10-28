#include <barrelfish/group_api.h>
#include <barrelfish/barrelfish.h>
#include <barrelfish/monitor_client.h>
#include <if/monitor_defs.h>
#include <barrelfish/dispatcher_arch.h>
#include <barrelfish/spawn_client.h>
#include <barrelfish/nameservice_client.h>

bool attach_complete;
static void attach_reply(struct monitor_binding *b, coreid_t succ_core) {
    attach_complete = true;
}

errval_t attach_group(coreid_t target_core, groupid_t target_group) {
    struct monitor_binding *mb = get_monitor_binding();
 
    mb->rx_vtbl.attach_group_reply = attach_reply;

    attach_complete = false;
    errval_t err = monitor_attach_group_request__tx(mb, NOP_CONT, target_core, target_group);
    if (err_is_fail(err)) {
        return err;
    }
    while (1) {
        messages_wait_and_handle_next();
        if (attach_complete) {
            break;
        }
    }

    return SYS_ERR_OK;
}

struct thread_entry_arg {
    void* arg;
    uint64_t affinities;
    thread_func_t entry;
};
static int thread_with_affinity_entry_wrapper(void* args)
{
    struct thread_entry_arg* wrapper_arg = (struct thread_entry_arg*)args;
    while (!(1 << get_core_id() & wrapper_arg->affinities));
    return wrapper_arg->entry(wrapper_arg->arg);
}

struct thread* thread_create_with_affinity(thread_func_t entry, void* arg, uint64_t affinities)
{
    struct thread_entry_arg* wrapper_arg = (struct thread_entry_arg*)malloc(sizeof(struct thread_entry_arg));
    wrapper_arg->arg = arg;
    wrapper_arg->affinities = affinities;
    wrapper_arg->entry = entry;
    struct thread* t = thread_create(thread_with_affinity_entry_wrapper, (void*)wrapper_arg);
    thread_set_affinity(t, affinities);
    return t;
}
