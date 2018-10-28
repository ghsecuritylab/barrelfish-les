/** \file
 *  \brief Hello World application
 */

/*
 * Copyright (c) 2010, 2011, 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <xmpl_group.h>

#define N 40

struct data_t {
	int a[N][N];
	int b[N][N];
	int result[N][N];
} mat;

static void single_thread(int thread_cnt, int thread_id, void* data)
{
	struct data_t *p = data;
	int i, j, m;
	for (i = 0;i < N;i++) {
		if (i % thread_cnt == thread_id) {
			for (j = 0; j < N; j++) {
				for (m = 0;m < N;m++) {
					p->result[i][j] += p->a[i][m] * p->b[m][j];
				}
			}
		}
	}
}

static void* prepare_data(void* buf, int type) {
	if (type == TEST_TYPE_SHARED_MEM) {
		*(struct data_t*)buf = mat;
		return buf;
	} else if (type == TEST_TYPE_CORE_FUSION) {
		return &mat;
	}
	assert(false);
	return NULL;
}

int main(int argc, char *argv[]) 
{
	testcase_t testcases[] ={
		{ "Matrix multiply", single_thread, prepare_data, },
	};
	set_testcases(testcases, 1);
	benchmark_entry(argc, argv, (int[]){2, 3, -1});
	benchmark_start_testcase(0, 3);

	group_entry((int[]){2, 3, -1});
	group_benchmark_start_testcase(0, 3);
	printf("start detach\n");
	group_end();
	return 0;
}
