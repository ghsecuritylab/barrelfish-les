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

void attach_group(coreid_t target_core, groupid_t target_group) {
    struct monitor_binding *mb = get_monitor_binding();
 
    mb->rx_vtbl.attach_group_reply = attach_reply;

    attach_complete = false;
    monitor_attach_group_request__tx(mb, NOP_CONT, target_core, target_group);
    while (1) {
        messages_wait_and_handle_next();
        if (attach_complete) {
            break;
        }
    }
}
