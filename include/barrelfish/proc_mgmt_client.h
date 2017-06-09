/**
 * \file
 * \brief Client for interacting with the process management server.
 */

/*
 * Copyright (c) 2017, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef BARRELFISH_PROC_MGMT_CLIENT_H
#define BARRELFISH_PROC_MGMT_CLIENT_H

#include <if/proc_mgmt_defs.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

struct proc_mgmt_lmp_binding;

errval_t proc_mgmt_client_lmp_accept(struct proc_mgmt_lmp_binding *lmpb,
                                     struct waitset *ws,
                                     size_t lmp_buflen_words);
errval_t proc_mgmt_client_lmp_bind(struct proc_mgmt_lmp_binding *lmpb,
                                   struct capref ep,
                                   proc_mgmt_bind_continuation_fn *cont,
                                   void *st,
                                   struct waitset *ws,
                                   size_t lmp_buflen_words);
errval_t proc_mgmt_bind_client(void);

errval_t proc_mgmt_add_spawnd(iref_t iref, coreid_t core_id);

__END_DECLS

#endif // BARRELFISH_PROC_MGMT_CLIENT_H
