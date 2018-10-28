#include <xmpl_group.h>

static struct capref frame_cap;
static size_t frame_size = 8 << 20; // 8MB
lvaddr_t frame_vaddr = 0;

static volatile int core_cnt = 0;
static volatile int connected_client_count;
static struct xmpl_group_binding* core_binding_map[MAX_CORE] = { NULL, };
static cycles_t communication_start_tsc[MAX_CORE];

static const char *service_name = "xmpl_group_service";

struct shared_mem_state_s {
    volatile int complete_client_count;
    struct {
        int thread_id;
    } thread_args[MAX_CORE];
};

static struct shared_mem_state_s* get_shared_mem_state(void) {
    if (frame_vaddr == 0) {
        printf("core %d\n", (int)frame_vaddr);
    }
    assert(frame_vaddr != 0);
    return (struct shared_mem_state_s*)frame_vaddr;
}

static void* get_shared_mem_data_buf(void) {
    assert(frame_vaddr != 0);
    return (void*)(frame_vaddr + DATA_ZONE_OFFSET);
}


static void shmc_init_request(struct xmpl_group_binding *b, coreid_t id) 
{
    printf("request called with id %hhu\n", id);
    core_binding_map[id] = b;
    xmpl_group_shmc_init_reply__tx(b, NOP_CONT, frame_cap);
    printf("request finished\n");
}

static void shmc_init_reply(struct xmpl_group_binding *b, struct capref cap)
{
    frame_cap = cap;

    void* shared_buf;
    errval_t err = vspace_map_one_frame(&shared_buf, frame_size, frame_cap, NULL, NULL);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Failed to map frame in client\n");
    }
    frame_vaddr = (lvaddr_t)shared_buf;

    xmpl_group_shmc_init_client_ready__tx(b, NOP_CONT);
}

static void shmc_init_client_ready(struct xmpl_group_binding *b)
{
    connected_client_count++;
}

// client tell server compute complete
static void shmc_compute_complete(struct xmpl_group_binding *b, coreid_t coreid, uint64_t cost)
{
    struct shared_mem_state_s* state = get_shared_mem_state();
    state->complete_client_count++; // server involve in too.
    push_cost_line(coreid, "client compute time", cost);
    push_cost_line(coreid, "client communication time", bench_time_diff(communication_start_tsc[coreid], bench_tsc()));
}

// server tell client start computing
static void shmc_compute_start(struct xmpl_group_binding *b, int testcase_idx)
{
    assert(testcase_idx >= 0);
    struct shared_mem_state_s* state = get_shared_mem_state();
    printf("client %hhu running testcase %d\n", disp_get_core_id(), testcase_idx);
    cycles_t cost = run_testcase(testcase_idx, core_cnt, state->thread_args[disp_get_core_id()].thread_id, get_shared_mem_data_buf());
    xmpl_group_shmc_compute_complete__tx(b, NOP_CONT, disp_get_core_id(), cost);
}

struct xmpl_group_rx_vtbl rx_vtbl = {
    .shmc_init_request = shmc_init_request,
    .shmc_init_reply = shmc_init_reply,
    .shmc_init_client_ready = shmc_init_client_ready,
    .shmc_compute_complete = shmc_compute_complete,
    .shmc_compute_start = shmc_compute_start,
};

static void bind_cb(void *st, errval_t err, struct xmpl_group_binding *b)
{
    printf("bind service successfully\n");
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "bind failed");
    }

    // copy my message receive handler vtable to the binding
    b->rx_vtbl = rx_vtbl;

    printf("sending init request\n");
    xmpl_group_shmc_init_request__tx(b, NOP_CONT, disp_get_core_id());
    printf("init request sent\n");
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "sending shm_init_request failed");
    }
}

static void export_cb(void *st, errval_t err, iref_t iref)
{
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "export failed");
    }

    // register this iref with the name service
    err = nameservice_register(service_name, iref);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "nameservice_register failed");
    }

    printf("Exported and registered\n");
}

static errval_t connect_cb(void *st, struct xmpl_group_binding *b)
{
    printf("client connected\n");
    b->rx_vtbl = rx_vtbl;
    return SYS_ERR_OK;
}

void benchmark_entry(int argc, char* argv[], int cores[])
{
    errval_t err;
    core_cnt = 0;
    for (int i = 0; i < MAX_CORE; i++) {
        if (cores[i] < 0)
            break;
        core_cnt++;
    }

    if ((argc >= 2) && (strcmp(argv[1], "client") == 0)) {
        // Lookup service
        printf("looking up service\n");
        iref_t iref;
        err = nameservice_blocking_lookup(service_name, &iref);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "nameservice_blocking_lookup failed");
        }
        printf("bind service\n");

        // Bind to service
        err = xmpl_group_bind(iref, bind_cb, NULL, get_default_waitset(),
                         IDC_BIND_FLAGS_DEFAULT);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "bind failed");
        }
        while (1) {
            messages_wait_and_handle_next();
        }
    } else {
        char path[MAXPATH];
        snprintf(path, MAXPATH, "examples/%s", argv[0]);

        connected_client_count = 0;

        char *argv2[] = {
            argv[0],
            "client",
            NULL,
        };

        size_t ret_size;
        err = frame_alloc(&frame_cap, frame_size, &ret_size);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "Failed to alloc frame\n");
        }

        void* shared_buf;
        err = vspace_map_one_frame(&shared_buf, frame_size, frame_cap, NULL, NULL);
        if (err_is_fail(err) || !shared_buf) {
            USER_PANIC_ERR(err, "Failed to map frame\n");
        }
        frame_vaddr = (lvaddr_t)shared_buf;
        printf("frame_vaddr is %d in core %hhu\n", frame_vaddr, disp_get_core_id());

        printf("spawning program\n");
        struct shared_mem_state_s* state = get_shared_mem_state();
        state->thread_args[disp_get_core_id()].thread_id = 0;
        for (int i = 0; i < MAX_CORE; i++) {
            if (cores[i] < 0)
                break;
            state->thread_args[cores[i]].thread_id = i + 1;
            printf("spawning program in core %d\n", cores[i]);
            err = spawn_program(cores[i], path, argv2, NULL, 0, 
                                            NULL);
            if (err_is_fail(err)) {
                USER_PANIC_ERR(err, "failed spawn on core %d", cores[i]);
            } else {
                debug_printf("program on core %"PRIuCOREID" spawned \n", cores[i]);
            }
        }
        printf("spawning finished\n");

        err = xmpl_group_export(NULL, export_cb, connect_cb, get_default_waitset(),
                          IDC_EXPORT_FLAGS_DEFAULT);

        while (connected_client_count != core_cnt) {
            messages_wait_and_handle_next();
        }
    }
}

void benchmark_start_testcase(int testcase_i, int repeat)
{
    struct shared_mem_state_s* state = get_shared_mem_state();

    assert(run_prepare_data(testcase_i, get_shared_mem_data_buf(), TEST_TYPE_SHARED_MEM) == get_shared_mem_data_buf());

    while (repeat--) {
        clear_cost_table();
        state->complete_client_count = 0;
        for (int i = 0; i < MAX_CORE; i++) {
            struct xmpl_group_binding* b = core_binding_map[i];
            if (b) {
                communication_start_tsc[i] = bench_tsc();
                xmpl_group_shmc_compute_start__tx(b, NOP_CONT, testcase_i);
            }
        }
        cycles_t cost = run_testcase(testcase_i, core_cnt, state->thread_args[disp_get_core_id()].thread_id, get_shared_mem_data_buf());
        state->complete_client_count++; // server involve in too.
        push_cost_line(disp_get_core_id(), "server compute cost", cost);
        while (state->complete_client_count != connected_client_count + 1) { // server involves in too.
            messages_wait_and_handle_next();
        }

        printf("testcase %d complete\n", testcase_i);
        gather_cost_stat(testcase_i);
    }
}
