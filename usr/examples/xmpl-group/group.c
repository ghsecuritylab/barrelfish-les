#include <xmpl_group.h>
#include <barrelfish/debug.h>

struct thread_arg {
    int thread_id;
} thread_args[MAX_CORE];

static volatile int test = 0xabcdef, cur_testcase_idx = 0;
static volatile void* cur_data = NULL;
static volatile int core_cnt = 0, ready_cnt = 0, finish_cnt = 0, pre_start_cnt = 0;
static volatile bool start_benchmark = false, finish_benchmark = false;
static int threadid_coreid_map[MAX_CORE];
static coreid_t core_list[MAX_CORE];

static void combine_group_reply(struct monitor_binding *b, monitor_groupid_t target_id) {
    ready_cnt++;
}

static void cont(void* arg) {
}

static int single_thread(void* arg)
{
    int thread_cnt = core_cnt;
    int thread_id = threadid_coreid_map[get_core_id()];
    while (1) {
        pre_start_cnt++;
        while (!start_benchmark);

        cycles_t compute_begin = bench_tsc();
        run_testcase(cur_testcase_idx, thread_cnt, thread_id, (void*)cur_data);
        cycles_t compute_end = bench_tsc();

        finish_cnt++;
        while (!finish_benchmark);
        push_cost_line(get_core_id(), "compute thread completed", bench_time_diff(compute_begin, compute_end));
    }
    return 0;
}

void group_entry(int cores[]) {
    struct monitor_binding *mb = get_monitor_binding();

    mb->rx_vtbl.combine_group_reply = combine_group_reply;
    core_cnt = 0;

    errval_t err;
    for (int i = 0; i < MAX_CORE; i++) {
        if (cores[i] < 0)
            break;
        core_list[i] = cores[i];
        core_cnt++;
        thread_create_compute(single_thread, cores[i], (void*)0x812);

        monitor_groupid_t target_group = cores[i];

        int origin_ready_cnt = ready_cnt;
        err = monitor_combine_group_request__tx(mb,MKCONT(cont, (void*)0xabc), target_group);
        while (ready_cnt != origin_ready_cnt + 1) {
            messages_wait_and_handle_next();
        }

        printf("combining %d completed %d\n", (int)target_group, err);
    }

    printf("init complete\n");
}

void group_benchmark_start_testcase(int testcase_i, int repeat)
{
    while (repeat--) {
        clear_cost_table();
        void* data = run_prepare_data(testcase_i, NULL, TEST_TYPE_CORE_FUSION);
        assert(cur_data = data);
        finish_cnt = 0;
        cur_testcase_idx = testcase_i;
        finish_benchmark = false;
        start_benchmark = true;
        // start computing .....
        cycles_t compute_start = bench_tsc();

        // computing.....
        run_testcase(testcase_i, core_cnt, 0, data);
        printf("main thread finished\n");

        // main thread finished

        cycles_t joined_start = bench_tsc();
        while (finish_cnt != core_cnt);

        // join finished....
        cycles_t joined_end = bench_tsc();

        pre_start_cnt = 0;
        start_benchmark = false;
        finish_benchmark = true;
        while (pre_start_cnt != core_cnt);

        push_cost_line(get_core_id(), "main thread compute cost", bench_time_diff(compute_start, joined_start));
        push_cost_line(get_core_id(), "main thread joined cost", bench_time_diff(joined_start, joined_end));
        printf("repeat is %d\n", repeat);

        gather_cost_stat(testcase_i);
    }
}
//
static void detach_core_reply(struct monitor_binding *b, coreid_t target_id) {
    printf("Coreid %hhu detached\n", target_id);
    core_cnt--;
}

void group_detach_cores(void)
{
    struct monitor_binding *mb = get_monitor_binding();
    mb->rx_vtbl.core_detach_reply = detach_core_reply;
    errval_t err;
    err = monitor_core_detach__tx(mb, NOP_CONT, core_list, core_cnt);
    if (err_is_fail(err)) {
      printf("Failed to tell monitor to detach core\n");
    } else {
      while (core_cnt) {
        messages_wait_and_handle_next();
      }
    }

    printf("detaching core completed\n");
}
