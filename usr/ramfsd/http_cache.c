/**
 * \file
 * \brief NFS-populated file cache for HTTP server
 *
 * This very stupid "cache" assumes that:
 *  All regular files in a hardcoded NFS mount point are cached at startup
 *  The contents of the cache never change nor expire
 */

/*
 * Copyright (c) 2008, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <nfs/nfs.h>
#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <trace/trace.h>
#include <trace_definitions/trace_defs.h>
#include <timer/timer.h>
#include <contmng/netbench.h>
#include "webserver_network.h"
#include "webserver_debug.h"
#include "webserver_session.h"
#include "http_cache.h"

/* NOTE: enable following #def if you want cache to be preloaded */
#define PRELOAD_WEB_CACHE 1

/* Enable tracing only when it is globally enabled */
#if CONFIG_TRACE && NETWORK_STACK_TRACE
//#define ENABLE_WEB_TRACING 1
#endif // CONFIG_TRACE && NETWORK_STACK_TRACE

//#define MAX_NFS_READ       14000
#define MAX_NFS_READ      1330 /* 14000 */ /* to avoid breakage of lwip*/

/* Maximum staleness allowed */
#define MAX_STALENESS ((cycles_t)9000000)

static void (*init_callback)(void);




/* global states */
static struct nfs_fh3 nfs_root_fh;  /* reference to the root dir of NFS */
static struct nfs_client *my_nfs_client; /* The NFS client */

static struct http_cache_entry *cache_table = NULL; /*root cached entry array */
static struct http_cache_entry *error_cache = NULL; /* cache entry for error */


#ifdef PRELOAD_WEB_CACHE
/* Initial cache loading state variables */
static bool cache_loading_phase = false;
static bool readdir_complete = false;
static int cache_lookups_started = 0;
static int cache_loaded_counter = 0;
static int cache_loading_probs = 0;
static void handle_cache_load_done(void);
#endif // PRELOAD_WEB_CACHE

// Variables for time measurement for performance
static uint64_t last_ts = 0;

/* allocate the buffer and initialize it. */
static struct buff_holder *allocate_buff_holder (size_t len)
{
    struct buff_holder *result = NULL;
    result = (struct buff_holder *) malloc (sizeof (struct buff_holder));
    assert (result != NULL );
    memset (result, 0, sizeof(struct buff_holder));
    if ( len > 0) {
        result->data = malloc (len);
        assert (result->data != NULL);
    }
    else {
        // len == 0
        // assertion "e->hbuff->data != NULL" failed: file "../usr/ramfsd/http_cache.c", line 273,
        // malloc dummy buffer and hope its fine
        // alternatively, remove the assertion and hope its fine
        result->data = malloc (1);
        assert (result->data != NULL);
    }
    /* NOTE: 0 is valid length and used by error_cache */
    result->len = len;
    result->r_counter = 1; /* initiating ref_counter to 1, and using it as ref
            for free */
    return result;
} /* end function: allocate_buff_holder */

/* increments and returns the incremented value of the ref_counter for bh */
static long increment_buff_holder_ref (struct buff_holder *bh)
{
    ++bh->r_counter;
    return (bh->r_counter);
} /* end Function: increment_buff_holder_ref */

/* Decrements value of the ref_counter for bh
    if r_counter reaches zero then free all the memory */
long decrement_buff_holder_ref (struct buff_holder *bh)
{
    if (bh == NULL) {
        return 0;
    }

    --bh->r_counter;
    if (bh->r_counter > 0) {
        return (bh->r_counter);
    }

    if (bh->len > 0 && bh->data != NULL) {
        free (bh->data);
    }
    free (bh);
    return 0;
} /* end Function: increment_buff_holder_ref */


/* allocates the memory for the cacheline */
static struct http_cache_entry * cache_entry_allocate (void)
{
    struct http_cache_entry *e;
    e = (struct http_cache_entry *)
        malloc (sizeof(struct http_cache_entry));
    assert (e != NULL );
    memset (e, 0, sizeof(struct http_cache_entry));
    return e;
}

/* Function create_404_page_cache  creates error_cache entry,
    this entry will be used to reply when file is not found */
static void create_404_page_cache (void)
{
    error_cache = cache_entry_allocate();
    error_cache->hbuff = allocate_buff_holder (0);
    increment_buff_holder_ref (error_cache->hbuff);
    error_cache->valid = 1;
} /* end function: create_404_page_cache */

/* Finds the cacheline associated with given name
 if no cacheline exists, it will create one,
 copy name as the key for cacheline */
static struct http_cache_entry *find_cacheline (const char *name)
{
    struct http_cache_entry *e;
    int l;
    // FIXME: simple linear search
    for (e = cache_table; e != NULL; e = e->next) {
        if (strcmp(name, e->name) == 0) {
            DEBUGPRINT ("cache-hit for [%s] == [%s]\n", name, e->name);
            return e;
        }
    } /* end for : for each cacheline */
    /* create new cacheline */
    e = cache_entry_allocate();
    /* copying the filename */
    l = strlen (name);
    e->name = (char *)malloc(sizeof(char)*(l+1));
    assert(e->name != NULL);
    strcpy(e->name, name);
    DEBUGPRINT ("cache-miss for [%s] so, created [%s]\n", name, e->name);
    e->next = cache_table;
    cache_table = e;
    return e;
} /* end function: find_cacheline */

static void delete_cacheline_from_cachelist (struct http_cache_entry *target)
{
    struct http_cache_entry *prev;
    struct http_cache_entry *e;

    if (cache_table == target){
        cache_table = target->next;
        return;
    }

    // FIXME: simple linear search
    prev = NULL;
    for (e = cache_table; e != NULL; e = e->next) {
        if (e == target) {
            prev->next = e->next;
            return;
        }
        prev = e;
    } /* end for : for each cacheline */
} /* end function: delete_cacheline_from_cachelist */


static void trigger_callback (struct http_conn *cs, struct http_cache_entry *e)
{

    if ( cs->mark_invalid ){
        /* This http_conn is not good anymore (most probably, it received RST)*/
        DEBUGPRINT ("%d: ERROR: callback triggered on invalid conn\n",
            cs->request_no );
        return;
    }

    /* making the callback */
    cs->hbuff = e->hbuff;
    /* increasing the reference */
    increment_buff_holder_ref (cs->hbuff);
    cs->callback(cs);
} /* end function: trigger_callback */


/* send callback for each pending http_conn */
static void handle_pending_list(struct http_cache_entry *e)
{
    struct http_conn *cs = NULL;
    struct http_conn *next_cs = NULL;
    if(e == NULL) {
        printf("ERROR: handle_pending_list: null passed for cache entry\n");
        return;
    }
    if(e->conn == NULL){
//      printf("ERROR: handle_pending_list: no one waiting\n");
        return;
    }
    cs = e->conn;
    while (cs != NULL) {
        e->conn = cs->next;
        trigger_callback(cs, e);
        next_cs = cs->next;
        /* Copying the next of cs as following function can release the cs. */
        //decrement_http_conn_reference(cs);
        cs = next_cs;
    }

    /* clear the list attached to the cache */
    e->conn = NULL;
    e->last = NULL;
} /* end function : handle_pending_list */


static void read_callback (void *arg, struct nfs_client *client,
                          READ3res *result)
{
    struct http_cache_entry *e = arg;
    assert( e != NULL);

    assert (result != NULL);
    assert (result->status == NFS3_OK);
    READ3resok *res = &result->READ3res_u.resok;
    assert(res->count == res->data.data_len);

    if (e->hbuff->data == NULL) {
        printf("%s:%s:%d: e->hbuff->data is NULL for e->name=%s\n",
               __FILE__, __FUNCTION__, __LINE__, e->name);
    }
    assert (e->hbuff != NULL);
    assert (e->hbuff->data != NULL );

    if (e->hbuff->len < e->copied + res->data.data_len) {
        printf("%s:%s:%d: e->name = %s e->hbuff->len = %zu e->copied = %zu res->data.data_len = %d\n",
               __FILE__, __FUNCTION__, __LINE__,
               e->name, e->hbuff->len,  e->copied, res->data.data_len);
    }

    assert (e->hbuff->len >= e->copied + res->data.data_len);
    memcpy (e->hbuff->data + e->copied, res->data.data_val, res->data.data_len);
    e->copied += res->data.data_len;

//    DEBUGPRINT ("got response of len %d, filesize %lu\n",
//              res->data.data_len, e->copied);

    // free arguments
    xdr_READ3res(&xdr_free, result);

    if (!res->eof) {
        // more data to come, read the next chunk
        err_t err = nfs_read(client, e->file_handle, e->copied, MAX_NFS_READ,
                        read_callback, e);
        assert(err == ERR_OK);
        return;
    }

    /* This is the end-of-file, so deal with it. */

    e->valid = 1;
    e->loading = 0;
    decrement_buff_holder_ref (e->hbuff);
    void place_file_in_ramfs(struct http_cache_entry*);
    place_file_in_ramfs(e);

#ifdef PRELOAD_WEB_CACHE
    if (!cache_loading_phase) {
        handle_pending_list (e); /* done! */
        return;
    }

    /* This is cache loading going on... */
    printf("Copied %zu bytes for file [%s] of length: %zu\n",
                e->copied, e->name, e->hbuff->len);
    ++cache_loaded_counter;
    handle_cache_load_done();

#else // PRELOAD_WEB_CACHE
    handle_pending_list(e); /* done! */
#endif // PRELOAD_WEB_CACHE
}

void create_all_the_dirs(struct http_cache_entry* e);

static void readdir_callback(void *arg, struct nfs_client *client, READDIR3res *result);
static void lookup_callback (void *arg, struct nfs_client *client,
                            LOOKUP3res *result)
{
    LOOKUP3resok *resok = &result->LOOKUP3res_u.resok;
    err_t r;
    struct http_cache_entry *e = arg;

    DEBUGPRINT ("inside lookup_callback_file for file %s\n", e->name);
    assert(result != NULL );
    DEBUGPRINT("we get a resok->obj_attributes.post_op_attr_u.attributes.type = %d\n",
               resok->obj_attributes.post_op_attr_u.attributes.type);

    /* initiate a read for every regular file */
    if ( result->status == NFS3_OK &&
        resok->obj_attributes.attributes_follow &&
        resok->obj_attributes.post_op_attr_u.attributes.type == NF3REG) {

        DEBUGPRINT("Copying %s of size %lu\n", e->name,
                    resok->obj_attributes.post_op_attr_u.attributes.size );

        /* Store the nfs directory handle in current_session (cs) */
        nfs_copyfh (&e->file_handle, resok->object);
        /* GLOBAL: Storing the global reference for cache entry */
        /* NOTE: this memory is freed in reset_cache_entry() */

        /* Allocate memory for holding the file-content */
        /* Allocate the buff_holder */
        e->hbuff = allocate_buff_holder(
            resok->obj_attributes.post_op_attr_u.attributes.size );
        /* NOTE: this memory will be freed by decrement_buff_holder_ref */

        /* increment buff_holder reference */
        increment_buff_holder_ref (e->hbuff);

        e->hbuff->len = resok->obj_attributes.post_op_attr_u.attributes.size;

        /* Set the size of the how much data is read till now. */
        e->copied = 0;

        r = nfs_read (client, e->file_handle, 0, MAX_NFS_READ,
                read_callback, e);
        assert (r == ERR_OK);

        // free arguments
        xdr_LOOKUP3res(&xdr_free, result);
        return;
    }

    if ( result->status == NFS3_OK &&
        resok->obj_attributes.attributes_follow &&
        resok->obj_attributes.post_op_attr_u.attributes.type == NF3DIR) {
        DEBUGPRINT("It's a directory yaya\n");
        char* basename = (strrchr(e->name, '/') != NULL) ?
                         strrchr(e->name, '/')+1 : e->name;
        if (!strcmp(basename, ".") || !strcmp(basename, "..")) {
            // Ignore those
            return;
        }

        create_all_the_dirs(e);

        nfs_copyfh (&e->file_handle, resok->object);

        // Recurse into it
        r = nfs_readdir(client, e->file_handle, NFS_READDIR_COOKIE,
                        NFS_READDIR_COOKIEVERF, readdir_callback, e);
        assert(r == ERR_OK);
        return;
    }

    /* Most probably the file does not exist */
    DEBUGPRINT ("Error: file [%s] does not exist, or wrong type [result->status = %d, type = %d]\n",
                e->name, result->status,
                resok->obj_attributes.post_op_attr_u.attributes.type);

#ifdef PRELOAD_WEB_CACHE
    if (cache_loading_phase){
        ++cache_loading_probs;
        handle_cache_load_done();
        return;
    }
#endif // PRELOAD_WEB_CACHE

    if (e->conn == NULL) {
        /* No connection is waiting for this loading */
        return;
    }

    /*  as file does not exist, send all the http_conns to error page. */
    error_cache->conn = e->conn;
    error_cache->last = e->last;
    handle_pending_list (error_cache); /* done! */

    /* free this cache entry as it is pointing to invalid page */
    e->conn = NULL;
    e->last = NULL;
    delete_cacheline_from_cachelist (e);
    if (e->name != NULL ) free(e->name);
    free(e);

    // free arguments
    xdr_LOOKUP3res(&xdr_free, result);
    return;
} /* end function: lookup_callback_file */

static err_t async_load_cache_entry(struct http_cache_entry *e, struct nfs_fh3 handle)
{
    err_t r;
    assert(e != NULL);

    // Do lookup for given filename in handle dir
    DEBUGPRINT ("pageloading starting\n");
    char* basename = (strrchr(e->name, '/') != NULL) ? strrchr(e->name, '/')+1 : e->name;
    if (!strcmp(basename, "..") || !strcmp(basename, ".")){
        return ERR_OK; // don't load those
    }
    printf("%s:%s:%d: nfs_lookup basename=%s\n",
           __FILE__, __FUNCTION__, __LINE__, basename);
    r = nfs_lookup(my_nfs_client, handle, basename,
                lookup_callback, e);
    assert(r == ERR_OK);
    return ERR_OK;
}





static void readdir_callback(void *arg, struct nfs_client *client,
                             READDIR3res *result)
{
    READDIR3resok *resok = &result->READDIR3res_u.resok;
    struct http_cache_entry *parent = NULL;
    struct http_cache_entry *ce;
    entry3 *last = NULL;
    err_t r;

    DEBUGPRINT ("readdir_callback came in\n");
    if(arg != NULL) {
        parent = (struct http_cache_entry *) arg;
        printf("%s:%s:%d: dealing with parent->name = %s\n",
               __FILE__, __FUNCTION__, __LINE__, parent->name);
    }
    printf("%s:%s:%d: results->status = %d\n",
           __FILE__, __FUNCTION__, __LINE__, result->status);
    assert(result != NULL && result->status == NFS3_OK);

    last_ts = rdtsc();
    // initiate a lookup for every entry
    for (entry3 *e = resok->reply.entries; e != NULL; e = e->nextentry) {
        ++cache_lookups_started;
        printf("Loading the file %s\n", e->name);

        char* real_path = calloc(1, 128); // leaked :(
        if (parent) {
            strcpy(real_path, parent->name);
            strcat(real_path, "/");
            strcat(real_path, e->name);
        }
        else {
            real_path = e->name;
        }
        ce = find_cacheline(real_path);
        ce->loading = 1;
        async_load_cache_entry(ce, (parent != NULL) ? parent->file_handle : nfs_root_fh);
        e->name = NULL; // prevent freeing by XDR
        last = e;
    }

    /* more in the directory: repeat call */
    if (!resok->reply.eof) {
        assert(last != NULL);
        r = nfs_readdir(client, (parent != NULL) ? parent->file_handle : nfs_root_fh,
                        last->cookie, resok->cookieverf, readdir_callback, parent);
        assert(r == ERR_OK);
    } else {
        readdir_complete = true;
        handle_cache_load_done();
    }

    // free arguments
    xdr_READDIR3res(&xdr_free, result);
}



static bool are_all_pages_loaded(void)
{
    if (readdir_complete == false ) {
        return false;
    }
    return (cache_lookups_started == (cache_loaded_counter + cache_loading_probs));
}

static void handle_cache_load_done(void)
{
    if (!are_all_pages_loaded()) {
        return;
    }
    DEBUGPRINT("initial_cache_load: entire cache loaded done\n");
    cache_loading_phase = false;



    // lwip_benchmark_control(1, BMS_STOP_REQUEST, 0, 0);
    // Report the cache loading time
    printf("Cache loading time %"PU"\n", in_seconds(rdtsc() - last_ts));
//    lwip_print_interesting_stats();

    /* continue with the web-server initialization. */
    init_callback(); /* do remaining initialization! */
}

static void initial_cache_load(struct nfs_client *client)
{
    err_t r;
    cache_loading_phase = true;
    cache_lookups_started = 0;
    cache_loaded_counter = 0;
    cache_loading_probs = 0;

    //my_nfs_client
    r = nfs_readdir(client, nfs_root_fh, NFS_READDIR_COOKIE,
                         NFS_READDIR_COOKIEVERF, readdir_callback, NULL);
    assert(r == ERR_OK);
}

static void mount_callback(void *arg, struct nfs_client *client,
                 enum mountstat3 mountstat, struct nfs_fh3 fhandle)
{
    assert(mountstat == MNT3_OK);
    nfs_root_fh = fhandle; /* GLOBAL: assigning root of filesystem handle */
    DEBUGPRINT ("nfs_mount successful, loading initial cache.\n");
    initial_cache_load(client); /* Initial load of files for cache. */
} /* end function: mount_callback */

err_t http_cache_init(struct ip_addr server, const char *path,
                     void (*callback)(void))
{
    init_callback = callback;

    my_nfs_client = nfs_mount(server, path, mount_callback, NULL);
    assert(my_nfs_client != NULL);

    cache_table = NULL;
    create_404_page_cache();

    DEBUGPRINT ("http_cache_init done\n");
    return ERR_OK;
} /* end function: http_cache_init */


