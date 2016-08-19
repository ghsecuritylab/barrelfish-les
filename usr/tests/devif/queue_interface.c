/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2011, 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <barrelfish/barrelfish.h>
#include <barrelfish/waitset.h>
#include <barrelfish/deferred.h>
#include <devif/queue_interface.h>

#define NUM_ENQ 8
#define NUM_ROUNDS 64
#define MEMORY_SIZE BASE_PAGE_SIZE*512

static struct capref memory;
static regionid_t regid;
static struct frame_identity id;
static lpaddr_t phys;

struct direct_state {
    struct list_ele* first;
    struct list_ele* last;
};

struct list_ele{
    regionid_t rid;
    bufferid_t bid;
    lpaddr_t addr;
    size_t len;
    uint64_t flags;
   
    struct list_ele* next;
};


errval_t create_direct(struct devq* q, uint64_t flags);
errval_t create_direct(struct devq* q, uint64_t flags)
{
    devq_allocate_state(q, sizeof(struct direct_state));
    return SYS_ERR_OK;
}

errval_t enqueue_direct(struct devq* q, regionid_t rid,
                        bufferid_t bid, lpaddr_t addr, size_t len,
                        uint64_t flags);
errval_t enqueue_direct(struct devq* q, regionid_t rid,
                        bufferid_t bid, lpaddr_t addr, size_t len,
                        uint64_t flags)
{
    struct direct_state* s = (struct direct_state*) devq_get_state(q);
    struct list_ele* ele;

    ele = (struct list_ele*) malloc(sizeof(struct list_ele));
    ele->rid = rid;
    ele->bid = bid;
    ele->addr = addr;
    ele->len = len;
    ele->flags = flags;
    ele->next = NULL;

    if (s->first == NULL) {
        s->first = ele;
        s->last = ele;
    } else {
        s->last->next = ele;
        s->last = ele;
    }
    return SYS_ERR_OK;
}

errval_t dequeue_direct(struct devq* q, regionid_t* rid,
                        bufferid_t* bid, lpaddr_t* addr, size_t* len,
                        uint64_t* flags);
errval_t dequeue_direct(struct devq* q, regionid_t* rid,
                        bufferid_t* bid, lpaddr_t* addr, size_t* len,
                        uint64_t* flags)
{
    struct direct_state* s = (struct direct_state*) devq_get_state(q);
    struct list_ele* ele;

    if (s->first == NULL) {
        return DEVQ_ERR_RX_EMPTY;
    } else {
        *rid = s->first->rid;
        *bid = s->first->bid;
        *addr = s->first->addr;
        *len = s->first->len;
        *flags = s->first->flags;
        ele = s->first;
        s->first = s->first->next;
        //free(ele);
        printf("Rid %d \n", *rid);
    }
    return SYS_ERR_OK;
}


errval_t register_direct(struct devq* q, struct capref cap,
                         regionid_t rid);
errval_t register_direct(struct devq* q, struct capref cap,
                         regionid_t rid) 
{
    return SYS_ERR_OK;
}


errval_t deregister_direct(struct devq* q, regionid_t rid);
errval_t deregister_direct(struct devq* q, regionid_t rid) 
{
    return SYS_ERR_OK;
}

errval_t notify_normal(struct devq* q, uint8_t num_slots);
errval_t notify_normal(struct devq* q, uint8_t num_slots)
{
    errval_t err;
    struct devq_buf* bufs = malloc(sizeof(struct devq_buf)*num_slots);
    for (int i = 0; i < num_slots; i++) {
        err = devq_dequeue(q, &(bufs[i].rid), &(bufs[i].addr),
                           &(bufs[i].len), &(bufs[i].bid), &(bufs[i].flags));
        if (err_is_fail(err)) {
            return err;
        }
    }
    return SYS_ERR_OK;
}

static void test_normal_device(void) 
{

    errval_t err;
    struct devq* q;   

    struct devq_func_pointer f = {
        .notify = notify_normal,
    };

    struct endpoint_state my_state = {
        .endpoint_type = ENDPOINT_TYPE_USER,
        .device_name = "my_queue",
        .q = NULL,
        .features = 0,
        .f = f,
    };

    printf("Normal device test started \n");
    err = devq_create(&q, &my_state, "loopback", 1);
    if (err_is_fail(err)){
        USER_PANIC("Allocating devq failed \n");
    }    

    err = devq_register(q, memory, &regid);
    if (err_is_fail(err)){
        USER_PANIC("Registering memory to devq failed \n");
    }
    
    bufferid_t ids[NUM_ENQ];
    for (int j = 0; j < NUM_ROUNDS; j++) {
        for (int i = 0; i < NUM_ENQ; i++) {
            lpaddr_t addr = phys+(j*NUM_ENQ*2048+i*2048);
            err = devq_enqueue(q, regid, addr, 2048, 
                               1, &ids[i]);
            if (err_is_fail(err)){
                USER_PANIC("Devq enqueue failed \n");
            }    
        }
        
        err = devq_notify(q);
        if (err_is_fail(err)) {
            printf("%s",err_getstring(err));
        }
        event_dispatch(get_default_waitset());
    }
    
    err = devq_control(q, 1, 1);
    if (err_is_fail(err)){
        USER_PANIC("Devq deregister failed \n");
    }

    err = devq_deregister(q, regid, &memory);
    if (err_is_fail(err)){
        USER_PANIC("Devq deregister failed \n");
    }

    err = devq_destroy(q);

    printf("Normal device test ended\n");
}

static void test_direct_device(void) 
{

    errval_t err;
    struct devq* q;   

    struct devq_func_pointer f = {
        .create = create_direct,
        .enq = enqueue_direct,
        .deq = dequeue_direct,
        .reg = register_direct,
        .dereg = deregister_direct,
    };

    struct endpoint_state my_state = {
        .endpoint_type = ENDPOINT_TYPE_DIRECT,
        .device_name = "my_queue",
        .q = NULL,
        .features = 0,
        .f = f,
    };

    printf("Direct device test started \n");
    err = devq_create(&q, &my_state, "loopback", 1);
    if (err_is_fail(err)){
        USER_PANIC("Allocating devq failed \n");
    }    

    err = devq_register(q, memory, &regid);
    if (err_is_fail(err)){
        USER_PANIC("Registering memory to devq failed \n");
    }
    
    regionid_t rid;
    bufferid_t ids[NUM_ENQ];
    lpaddr_t addr;
    size_t len;
    uint64_t flags;
    for (int j = 0; j < NUM_ROUNDS; j++) {
        for (int i = 0; i < NUM_ENQ; i++) {
            addr = phys+(j*NUM_ENQ*2048+i*2048);
            err = devq_enqueue(q, regid, addr, 2048, 
                               1, &ids[i]);
            if (err_is_fail(err)){
                USER_PANIC("Devq enqueue failed \n");
            }    
        }
     
        for (int i = 0; i < NUM_ENQ; i++) {
            err = devq_dequeue(q, &rid, &addr, &len, &ids[i], &flags);
            if (err_is_fail(err)){
                USER_PANIC("Devq dequeue failed \n");
            }    
        }
    }
    
    err = devq_control(q, 1, 1);
    if (err_is_fail(err)){
        USER_PANIC("Devq deregister failed \n");
    }

    err = devq_deregister(q, regid, &memory);
    if (err_is_fail(err)){
        USER_PANIC("Devq deregister failed \n");
    }

    err = devq_destroy(q);

    printf("Direct device test ended\n");
}

int main(int argc, char *argv[])
{
    errval_t err;
    // Allocate memory
    err = frame_alloc(&memory, MEMORY_SIZE, NULL);
    if (err_is_fail(err)){
        USER_PANIC("Allocating cap failed \n");
    }    
    
    err = invoke_frame_identify(memory, &id);
    if (err_is_fail(err)) {
        USER_PANIC("Frame identify failed \n");
    }
    
    phys = id.base;

    test_normal_device();
    test_direct_device();
}

