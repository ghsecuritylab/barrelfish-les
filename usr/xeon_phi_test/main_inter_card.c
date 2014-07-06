/*
 * Copyright (c) 2007-12 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */
#include <barrelfish/barrelfish.h>
#include <barrelfish/ump_chan.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <xeon_phi/xeon_phi_messaging.h>
#include <xeon_phi/xeon_phi_messaging_client.h>
#include <xeon_phi/xeon_phi_dma_client.h>

uint32_t send_reply = 0x0;

#include "benchmark.h"

uint8_t connected = 0;

static void *local_buf;
static struct capref local_frame;
static lpaddr_t local_base;
static size_t local_frame_sz;

static void *remote_buf;
static struct capref remote_frame;
static lpaddr_t remote_base;
static size_t remote_frame_sz;

static struct ump_chan uc;
static struct ump_chan uc_rev;

static void *inbuf;
static void *outbuf;

static void *inbuf_rev;
static void *outbuf_rev;

static struct bench_bufs bufs;
static struct bench_bufs bufs_rev;

static void init_buffer_c0(void)
{
#ifdef XPHI_BENCH_CHAN_HOST
    inbuf = local_buf + XPHI_BENCH_MSG_FRAME_SIZE;
    outbuf = local_buf;
    inbuf_rev = remote_buf + XPHI_BENCH_MSG_FRAME_SIZE;;
    outbuf_rev = remote_buf;
#endif

#ifdef XPHI_BENCH_CHAN_CARD
    inbuf = remote_buf;
    outbuf = remote_buf + XPHI_BENCH_MSG_FRAME_SIZE;
    inbuf_rev = local_buf;
    outbuf_rev = local_buf + XPHI_BENCH_MSG_FRAME_SIZE;
#endif

#ifdef XPHI_BENCH_CHAN_DEFAULT
    inbuf = remote_buf;
    outbuf = local_buf;
    inbuf_rev = outbuf+XPHI_BENCH_MSG_FRAME_SIZE;
    outbuf_rev = inbuf+XPHI_BENCH_MSG_FRAME_SIZE;
#endif

#ifdef XPHI_BENCH_BUFFER_CARD
    bufs.buf = remote_buf + 2 * XPHI_BENCH_MSG_FRAME_SIZE;
    bufs_rev.buf = local_buf + 2 * XPHI_BENCH_MSG_FRAME_SIZE;
#else
    bufs.buf = local_buf + 2 * XPHI_BENCH_MSG_FRAME_SIZE;
    bufs_rev.buf = remote_buf + 2 * XPHI_BENCH_MSG_FRAME_SIZE;
#endif
}

static errval_t alloc_local(void)
{
    errval_t err;

    size_t frame_size = 0;
    if (disp_xeon_phi_id() == 0) {
        frame_size = XPHI_BENCH_FRAME_SIZE_HOST;
    } else {
        frame_size = XPHI_BENCH_FRAME_SIZE_CARD;
    }

    if (!frame_size) {
        frame_size = 4096;
    }

    debug_printf("Allocating a frame of size: %lx\n", frame_size);

    size_t alloced_size = 0;
    err = frame_alloc(&local_frame, frame_size, &alloced_size);
    assert(err_is_ok(err));
    assert(alloced_size >= frame_size);

    struct frame_identity id;
    err = invoke_frame_identify(local_frame, &id);
    assert(err_is_ok(err));
    local_base = id.base;
    local_frame_sz = alloced_size;

    err = vspace_map_one_frame(&local_buf, alloced_size, local_frame, NULL, NULL);

    return err;
}

static errval_t msg_open_cb(struct capref msgframe,
                            uint8_t chantype)
{
    errval_t err;

    struct frame_identity id;
    err = invoke_frame_identify(msgframe, &id);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "could not identify the frame");
    }

    debug_printf("msg_open_cb | Frame base: %016lx, size=%lx\n",
                 id.base,
                 1UL << id.bits);

    remote_frame = msgframe;

    remote_base = id.base;

    remote_frame_sz = (1UL << id.bits);

    err = vspace_map_one_frame(&remote_buf, remote_frame_sz, msgframe,
    NULL,
                               NULL);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Could not map the frame");
    }

    init_buffer_c0();

    connected = 0x1;

    return SYS_ERR_OK;
}

static struct xeon_phi_messaging_cb callbacks = {
    .open = msg_open_cb
};

int main(int argc,
         char **argv)
{
    errval_t err;

    debug_printf("Xeon Phi Test started on the card %u.\n", disp_xeon_phi_id());

    debug_printf("Msg Buf Size = %lx, Buf Frame Size = %lx\n",
    XPHI_BENCH_MSG_FRAME_SIZE,
                 XPHI_BENCH_BUF_FRAME_SIZE);

    err = xeon_phi_messaging_service_init(&callbacks);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "could not init the service\n");
    }

    err = alloc_local();
    assert(err_is_ok(err));

    err = xeon_phi_messaging_service_start(XEON_PHI_MESSAGING_NO_HANDLER);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "could not start the service\n");
    }

    char iface[30];
    snprintf(iface, 30, "xeon_phi_inter.%u", 2);

    if (disp_xeon_phi_id() == 0) {
        debug_printf("sending open message to %s on node 1\n", iface);
        err = xeon_phi_messaging_open(1, iface, local_frame, XEON_PHI_CHAN_TYPE_UMP);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "could not open channel");
        }
    }

    while (!connected) {
        messages_wait_and_handle_next();
    }

    debug_printf("Initializing UMP channel...\n");

    if (disp_xeon_phi_id() != 0) {
        debug_printf("sending open message to %s on node 0\n", iface);
        err = xeon_phi_messaging_open(0, iface, local_frame,
        XEON_PHI_CHAN_TYPE_UMP);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "could not open channel");
        }
    } else {
        debug_printf("Other node reply: %s\n", (char *) local_buf);
    }

    err = ump_chan_init(&uc, inbuf,
                        XPHI_BENCH_MSG_FRAME_SIZE,
                        outbuf,
                        XPHI_BENCH_MSG_FRAME_SIZE);
    err = ump_chan_init(&uc_rev, inbuf_rev,
                            XPHI_BENCH_MSG_FRAME_SIZE,
                            outbuf_rev,
                            XPHI_BENCH_MSG_FRAME_SIZE);

    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Could not initialize UMP");
    }

    if (disp_xeon_phi_id() == 1) {
#ifndef XPHI_BENCH_THROUGHPUT
        debug_printf("---------------- normal run -----------------\n");
        xphi_bench_start_initator_rtt(&bufs, &uc);
        debug_printf("---------------- reversed run -----------------\n");
        xphi_bench_start_initator_rtt(&bufs_rev, &uc_rev);
#else
#ifdef XPHI_BENCH_SEND_SYNC
        debug_printf("---------------- normal run -----------------\n");
        xphi_bench_start_initator_sync(&bufs, &uc);
        debug_printf("---------------- reversed run -----------------\n");
        xphi_bench_start_initator_sync(&bufs_rev, &uc_rev);
#else
        debug_printf("---------------- normal run -----------------\n");
        xphi_bench_start_initator_async(&bufs, &uc);
        debug_printf("---------------- reversed run -----------------\n");
        xphi_bench_start_initator_async(&bufs_rev, &uc_rev);
#endif
#endif
    } else {
#ifndef XPHI_BENCH_THROUGHPUT
        debug_printf("---------------- normal run -----------------\n");
        xphi_bench_start_echo(&bufs, &uc);
        debug_printf("---------------- reversed run -----------------\n");
        xphi_bench_start_echo(&bufs_rev, &uc_rev);
#else
        debug_printf("---------------- normal run -----------------\n");
        xphi_bench_start_processor(&bufs, &uc);
        debug_printf("---------------- reversed run -----------------\n");
        xphi_bench_start_processor(&bufs_rev, &uc_rev);
#endif
    }

    err = xeon_phi_dma_client_register(0, local_frame);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "could not register memory");
    }

    err = xeon_phi_dma_client_register(0, remote_frame);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "could not register memory");
    }

    if (disp_xeon_phi_id() == 1) {
        debug_printf("+++++++ DMA / MEMCOPY Benchmark ++++++++\n");

        debug_printf("---------- card local ---------\n");
        xphi_bench_memcpy(local_buf + 2* XPHI_BENCH_MSG_FRAME_SIZE,
                          local_buf + 2* XPHI_BENCH_MSG_FRAME_SIZE
                          + (XPHI_BENCH_BUF_FRAME_SIZE / 2),
                          XPHI_BENCH_BUF_FRAME_SIZE / 2,
                          local_base + 2* XPHI_BENCH_MSG_FRAME_SIZE,
                          local_base + 2* XPHI_BENCH_MSG_FRAME_SIZE
                          + (XPHI_BENCH_BUF_FRAME_SIZE / 2));

        debug_printf("---------- card -> card ---------\n");
        xphi_bench_memcpy(local_buf + 2* XPHI_BENCH_MSG_FRAME_SIZE,
                          remote_buf + 2* XPHI_BENCH_MSG_FRAME_SIZE,
                          XPHI_BENCH_BUF_FRAME_SIZE / 2,
                          local_base + 2* XPHI_BENCH_MSG_FRAME_SIZE,
                          remote_base + 2* XPHI_BENCH_MSG_FRAME_SIZE);
    }

    debug_printf("benchmark done.");

    while (1) {
        messages_wait_and_handle_next();
    }
}

