#include <xmpl_group.h>

static struct cost_s core_cost_map[MAX_CORE];
testcase_t testcase_arr[128];

void gather_cost_stat(int idx)
{
    const char* testcase_name = get_testcase(idx).name;
    printf("testcase: %s\n", testcase_name);
    for (int core = 0; core < MAX_CORE; core++) {
        struct cost_s* cur_costs = &core_cost_map[core];
        for (int c = 0; c < cur_costs->cost_count; c++) {
            printf("core %d, cost description %s, cost %llu\n", core, cur_costs->cost[c].description, bench_tsc_to_ms(cur_costs->cost[c].cost));
        }
    }
}

cycles_t run_testcase(int testcase_idx, int thread_cnt, int thread_id, void *buf)
{
    testcase_t testcase = testcase_arr[testcase_idx];
	cycles_t bench_start = bench_tsc();
    testcase.compute_func(thread_cnt, thread_id, buf);
	cycles_t bench_end = bench_tsc();
    return bench_time_diff(bench_start, bench_end);
}

void push_cost_line(coreid_t coreid, const char* dscp, cycles_t cost)
{
    struct cost_s* cost_line = &core_cost_map[coreid];
    assert(cost_line->cost_count < MAX_COST_NUM);
    cost_line->cost[cost_line->cost_count].description = dscp;
    cost_line->cost[cost_line->cost_count].cost = cost;
    cost_line->cost_count++;
}

void set_testcases(testcase_t testcases[], int len)
{
    for (int i = 0; i < len; i++) {
        testcase_arr[i] = testcases[i];
    }
}

void clear_cost_table(void) {
    memset(core_cost_map, 0, sizeof(core_cost_map));
}

testcase_t get_testcase(int idx) {
    return testcase_arr[idx];
}

void* run_prepare_data(int idx, void* data, int type) {
    testcase_t testcase = get_testcase(idx);
    assert(testcase.prepare_data && testcase.compute_func);

    return testcase.prepare_data(data, type);
}
