#include <barrelfish/barrelfish.h>
#include <barrelfish/monitor_client.h>
#include <if/monitor_defs.h>
#include <barrelfish/dispatcher_arch.h>
#include <bench/bench.h>
#include <barrelfish/spawn_client.h>
#include <barrelfish/nameservice_client.h>

const int target_core = 2;
bool attach_complete;
static void attach_reply(struct monitor_binding *b, coreid_t succ_core) {
    printf("Coreid %hhu attached\n", succ_core);
    attach_complete = true;
}

static void attach_test(int target) {
    struct monitor_binding *mb = get_monitor_binding();
 
    mb->rx_vtbl.attach_group_reply = attach_reply;

    attach_complete = false;
    monitor_attach_group_request__tx(mb, NOP_CONT, target, get_core_id());
    while (1) {
        messages_wait_and_handle_next();
        if (attach_complete) {
            break;
        }
    }
}

static int test_thread(void* args) {
    int count = 5;
    if ((int)args == 10) {
        printf("Waiting to be scheduled by core 2\n");
        int c = 0;
        while (get_core_id() != 2) {
            c++;
            if (c % 9000000 * 5 == 0) {
                printf("waiting circle %d\n", c);
            }
        }
    }
    if ((int)args == 40) {
        thread_set_affinity(thread_self(), 1 << 2);
    }
    while (count--) {
        printf("thread %d %d\n", get_core_id(), (int)args);
        for (int i = 0; i < 9000000 * 5; i++);
        if ((int)args == 10 || (int)args == 40) {
            if (count == 3) {
                disp_disable();
                printf("migrated...thread %d %d\n", get_core_id(), (int)args);
                disp_enable(curdispatcher());
                printf("migrated back...thread %d %d\n", get_core_id(), (int)args);
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct thread *a = thread_create(test_thread, (void *)10);
    struct thread *b = thread_create(test_thread, (void *)20);

    int target = 2;

    thread_set_affinity(a, 1 << target);
    attach_test(target);

    thread_join(a, NULL);
    thread_join(b, NULL);

    printf("thread join succ\n");

    a = thread_create(test_thread, (void *)30);
    b = thread_create(test_thread, (void *)40);
    thread_join(a, NULL);
    thread_join(b, NULL);
    printf("exit group test\n");

    return 0;
}