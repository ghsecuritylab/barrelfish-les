#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <barrelfish/barrelfish.h>
#include <barrelfish/monitor_client.h>
#include <if/monitor_defs.h>
#include <barrelfish/dispatcher_arch.h>
#include <bench/bench.h>
#include <barrelfish/spawn_client.h>
#include <barrelfish/nameservice_client.h>
#include <if/xmpl_group_defs.h>

#define MAXPATH 256
#define MAX_CORE 16
#define MAX_COST_NUM 16
#define DATA_ZONE_OFFSET 4096

#define TEST_TYPE_SHARED_MEM 1
#define TEST_TYPE_CORE_FUSION 2

// for testcase.c
typedef struct testcase_s {
    const char* name;
	void (*compute_func)(int thread_cnt, int thread_id, void *data_buf);
	void* (*prepare_data)(void* data_buf, int type);
} testcase_t;

struct cost_s {
    struct {
        const char* description;
        cycles_t cost;
    } cost[MAX_COST_NUM];
    int cost_count;
};

void gather_cost_stat(int idx);
cycles_t run_testcase(int testcase_idx, int thread_cnt, int thread_id, void *buf);
void push_cost_line(coreid_t coreid, const char* dscp, cycles_t cost);
void clear_cost_table(void);
testcase_t get_testcase(int idx);
void* run_prepare_data(int idx, void* data, int type);

// for shared_mem.c
void benchmark_entry(int argc, char* argv[], int cores[]);
void set_testcases(testcase_t testcases[], int len);
void benchmark_start_testcase(int testcase_i, int repeat);


// for group.c
void group_entry(int cores[]);
void group_benchmark_start_testcase(int testcase_i, int repeat);
void group_detach_cores(void);
