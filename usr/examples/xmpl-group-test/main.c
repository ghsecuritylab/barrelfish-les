#include <barrelfish/barrelfish.h>
#include <barrelfish/monitor_client.h>
#include <if/monitor_defs.h>
#include <barrelfish/dispatcher_arch.h>
#include <bench/bench.h>
#include <barrelfish/spawn_client.h>
#include <barrelfish/nameservice_client.h>
#include <if/xmpl_group_defs.h>

static void attach_reply(struct monitor_binding *b, coreid_t succ_core) {
    printf("Coreid %hhu attached\n", succ_core);
}

static void attach_test(void) {
    struct monitor_binding *mb = get_monitor_binding();
 
    mb->rx_vtbl.attach_group_reply = attach_reply;

    monitor_attach_group_request__tx(mb, NOP_CONT, 2, get_core_id());
    while (1) {
        messages_wait_and_handle_next();
    }
}

int main(int argc, char *argv[])
{
    attach_test();
    return 0;
}