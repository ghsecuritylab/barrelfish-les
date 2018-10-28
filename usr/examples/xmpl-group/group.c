#include <barrelfish/debug.h>
#include <barrelfish/group_api.h>

#include <xmpl_group.h>

struct thread_arg {
    int thread_id;
} thread_args[MAX_CORE];

static volatile int test = 0xabcdef, cur_testcase_idx = 0;
static volatile void* cur_data = NULL;
static volatile int core_cnt = 0, ready_cnt = 0, finish_cnt = 0, pre_start_cnt = 0;
static volatile bool start_benchmark = false, finish_benchmark = false;
static int threadid_coreid_map[MAX_CORE];
static coreid_t core_list[MAX_CORE];
static struct thread* thread_list[MAX_CORE];

static volatile int benchmard_end = false;

static int single_thread(void* target_core)
{
    while (benchmard_end) {
        int thread_cnt = core_cnt;
        pre_start_cnt++;
        while (!start_benchmark);

        cycles_t compute_begin = bench_tsc();
        run_testcase(cur_testcase_idx, thread_cnt, threadid_coreid_map[get_core_id()], (void*)cur_data);
        cycles_t compute_end = bench_tsc();

        finish_cnt++;
        while (!finish_benchmark);
        push_cost_line(get_core_id(), "compute thread completed", bench_time_diff(compute_begin, compute_end));
    }
    return 0;
}

void group_entry(int cores[]) {
    benchmard_end = false;

    core_cnt = 0;
    errval_t err;
    for (int i = 0; i < MAX_CORE; i++) {
        if (cores[i] < 0)
            break;

        core_cnt++;
        core_list[i] = cores[i];
        thread_list[i] = thread_create_with_affinity(single_thread, NULL, 1 << cores[i]);
        threadid_coreid_map[cores[i]] = core_cnt;
        
        err = attach_group(cores[i], get_core_id());
        printf("ask core %d attach completed %d\n", cores[i] , err);
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

void group_end(void) {
    benchmard_end = true;
    while (core_cnt--) {
        thread_join(thread_list[core_cnt], NULL);
        attach_group(core_list[core_cnt], core_list[core_cnt]);
        printf("Core %d detached\n", core_list[core_cnt]);
    }
}