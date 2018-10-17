#include <barrelfish/barrelfish.h>
#include <barrelfish/group_api.h>
#include <bench/bench.h>

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
    attach_group(target, get_core_id());

    thread_join(a, NULL);
    thread_join(b, NULL);

    printf("thread join succ\n");

    a = thread_create(test_thread, (void *)30);
    b = thread_create(test_thread, (void *)40);
    thread_join(a, NULL);
    thread_join(b, NULL);

    attach_group(target, target);
    printf("exit group test\n");
    return 0;
}