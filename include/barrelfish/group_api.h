#ifndef BARRELFISH_GROUP_API_H
#define BARRELFISH_GROUP_API_H

#include <barrelfish/barrelfish.h>
errval_t attach_group(coreid_t target_core, groupid_t target_group);
struct thread* thread_create_with_affinity(thread_func_t entry, void* arg, uint64_t affinities);

#endif