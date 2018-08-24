#include <barrelfish/barrelfish.h>
#include "serial.h"
#include <barrelfish/inthandler.h>
#include <driverkit/driverkit.h>

#include <dev/jetsontk1/jetson_uart_dev.h>
#include <maps/jetsontk1_map.h>

//
// Serial console and debugger interfaces
// XXX: Fix me, not use!
//
#define NUM_PORTS 4

#define UART_IRQ (32+90)
#define UART_PORTD 3

#define CONFIG_SYS_NS16550_CLK 408000000

#define DIV_ROUND_CLOSEST(x, divisor)(          \
{                           \
        (((x) + ((divisor) / 2)) / (divisor));   \
}                           \
)

static int calc_divisor(jetson_uart_t *uart)
{
    const unsigned int mode_x_div = 16;

    return DIV_ROUND_CLOSEST(CONFIG_SYS_NS16550_CLK,
                        mode_x_div * 115200);
}

unsigned int serial_console_port = 3;
unsigned int serial_debug_port = 3;
unsigned const int serial_num_physical_ports = NUM_PORTS;


lpaddr_t uart_base[NUM_PORTS] = {
    JETSON_APB_UARTA,
    JETSON_APB_UARTB,
    JETSON_APB_UARTC,
    JETSON_APB_UARTD
};

size_t uart_size[NUM_PORTS] = {
    JETSON_APB_UARTA_SIZE,
    JETSON_APB_UARTB_SIZE,
    JETSON_APB_UARTC_SIZE,
    JETSON_APB_UARTD_SIZE
};


static jetson_uart_t ports[NUM_PORTS];
static bool uart_initialized[NUM_PORTS];

static void serial_poll(jetson_uart_t *uart)
{
    // Read while we can
    while(jetson_uart_LSR_rx_data_r_rdf(uart)) {
        char c = jetson_uart_RBR_rbr_rdf(uart);
        serial_input(&c, 1);
    }
}

static void serial_interrupt(void *arg)
{
    // We better be initialized!
    assert(uart_initialized[serial_console_port]);
    // XXX: this is probably not correct for all ports!
    uint16_t port = *(uint16_t*)arg;
    jetson_uart_t *uart = &ports[port];

    // get type
    jetson_uart_IIR_t iir =
        jetson_uart_IIR_rd(uart);

    if (jetson_uart_IIR_it_pending_extract(iir) == 0) {
        jetson_uart_it_type_t it_type = jetson_uart_IIR_it_type_extract(iir);
        switch(it_type) {
            case jetson_uart_it_modem:
                (void) jetson_uart_MSR_rd(uart);
                break;
            case jetson_uart_it_rxto:
            case jetson_uart_it_rda:
                serial_poll(uart);
                break;
            default:
                debug_printf("serial_interrupt: unhandled irq: %d\n", it_type);
                break;
        }
    }
}

#if 0
static bool convert_rx_simple(uint8_t *trig)
{
    switch(*trig) {
        case 8:
            *trig = 0;
            return true;
        case 16:
            *trig = 1;
            return true;
        case 56:
            *trig = 2;
            return true;
        case 60:
            *trig = 3;
            return true;
        default:
            return false;
    }
}
static bool convert_tx_simple(uint8_t *trig)
{
    switch(*trig) {
        case 8:
            *trig = 0;
            return true;
        case 16:
            *trig = 1;
            return true;
        case 32:
            *trig = 2;
            return true;
        case 56:
            *trig = 3;
            return true;
        default:
            return false;
    }
}
#endif
/*
 * Initialzie OMAP UART with interrupt
 * UART TRM 23.3
 */
static void jetson_uart_init(jetson_uart_t *uart, lvaddr_t base)
{
    // XXX: test this with other values
    // rx and tx FIFO threshold values (1 -- 63)
    //uint8_t rx_trig = 1; // amount of characters in fifo
    //uint8_t tx_trig = 63; // amount of free spaces in fifo
    //bool need_rx_1b = convert_rx_simple(&rx_trig);
    //bool need_tx_1b = convert_tx_simple(&tx_trig);

    int baud_divisor;

    baud_divisor = calc_divisor(uart);

    jetson_uart_initialize(uart, (mackerel_addr_t) base);
    while(!jetson_uart_LSR_tsr_e_rdf(uart));

    jetson_uart_LCR_dlab_wrf(uart, 1);

    jetson_uart_DLL_clock_lsb_wrf(uart, 0);
    jetson_uart_DLM_clock_msb_wrf(uart, 0);

    jetson_uart_LCR_dlab_wrf(uart, 0);

    jetson_uart_MCR_out2_wrf(uart, 1);
    jetson_uart_MCR_dtr_wrf(uart, 1);
    jetson_uart_MCR_rts_wrf(uart, 1);

    jetson_uart_FCR_t fcr = jetson_uart_FCR_default;

    fcr = jetson_uart_FCR_rx_fifo_trig_insert(fcr, 0);
    fcr = jetson_uart_FCR_tx_fifo_trig_insert(fcr, 0);
    fcr = jetson_uart_FCR_dma_mode_insert(fcr, 1); //DMA
    fcr = jetson_uart_FCR_fifo_en_insert(fcr, 1); // enable FIFOs
    jetson_uart_FCR_wr(uart, fcr);


    jetson_uart_FCR_tx_fifo_clear_wrf(uart, 1);
    jetson_uart_FCR_rx_fifo_clear_wrf(uart, 1);


    //5 clear IER register
    jetson_uart_IER_wr(uart, 0);
    //6 config mode B
    jetson_uart_LCR_dlab_wrf(uart, 1);
    //7 new divisor value --> 115200 baud == 0x00, 0x1A (dlh, dll)
    jetson_uart_DLL_clock_lsb_wrf(uart, baud_divisor & 0xff);
    jetson_uart_DLM_clock_msb_wrf(uart, (baud_divisor >> 8) & 0xff);
    //8 register operational mode
    jetson_uart_LCR_dlab_wrf(uart, 0);
    //9 load irq config --> only rhr irq for now
    jetson_uart_IER_rhr_it_wrf(uart, 1);
    //jetson_uart_IER_eord_it_wrf(uart, 1);
    //jetson_uart_IER_rx_timeout_it_wrf(uart, 1);
    jetson_uart_IER_modem_sts_it_wrf(uart, 1);
    //jetson_uart_IER_line_sts_it_wrf(uart, 1);


    jetson_uart_LCR_t lcr = jetson_uart_LCR_default;
    lcr = jetson_uart_LCR_parity_en_insert(lcr, 0);       // No parity
    lcr = jetson_uart_LCR_nb_stop_insert(lcr, 0);         // 1 stop bit
    lcr = jetson_uart_LCR_char_length_insert(lcr, jetson_uart_wl_8bits); // 8 data bits
    jetson_uart_LCR_wr(uart, lcr);
}


static errval_t real_init(unsigned port)
{
    if (port >= NUM_PORTS) {
        return SYS_ERR_SERIAL_PORT_INVALID;
    }
    if (uart_initialized[port]) {
        printf("jetsontk1 serial_init[%d]: already initialized; skipping.\n", port);
        return SYS_ERR_OK;
    }

    // XXX: TODO: figure this out --> kaluga magic?
    errval_t err;
    lvaddr_t vbase;
    err = map_device_register(uart_base[port], uart_size[port], &vbase);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "map_device_register failed\n");
        return err;
    }
    assert(vbase);

    // paging_map_device returns an address pointing to the beginning of
    // a section, need to add the offset for within the section again
    debug_printf("jetsontk1 serial_init base = 0x%"PRIxLVADDR"\n", vbase);
    jetson_uart_init(&ports[port], vbase);
    uart_initialized[port] = true;
    debug_printf("jetsontk1 serial_init[%d]: done.\n", port);
    return SYS_ERR_OK;
}

errval_t serial_init(struct serial_params *params)
{
	
#if 0
	uint32_t membase = (uint32_t) params->membase;
	uint32_t portbase = (uint32_t) params->portbase; 
    // ARM: we ignore the irq argument and use the portbase as UART port
    // number.
    if (portbase > NUM_PORTS) {
        debug_printf("don't have serial port #%d... exiting\n", portbase);
        return SYS_ERR_SERIAL_PORT_INVALID;
    }
#endif
	unsigned portbase = UART_PORTD;
    // initialize hardware
    errval_t err = real_init(portbase);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "serial_init failed\n");
        return -1;
    }

    // register interrupt
    uint16_t *pb = malloc(sizeof(uint16_t));
    *pb = portbase;
    err = inthandler_setup_arm(serial_interrupt, pb, UART_IRQ);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "interrupt setup failed.");
    }

    // offer service now we're up
    start_service();
	printf("strat_service done!\n");
    return SYS_ERR_OK;
}

/** output a single character */
static void serial_putchar(unsigned port, char c)
{
    assert(port <= NUM_PORTS);
    jetson_uart_t *uart = &ports[port];

    // Wait until FIFO can hold more characters
    while (!jetson_uart_LSR_thr_e_rdf(uart));
    // Write character
    jetson_uart_THR_thr_wrf(uart, c);
}

/** write string to serial port */
void serial_write(const char *c, size_t len)
{
    for (int i = 0; i < len; i++) {
        serial_putchar(serial_console_port, c[i]);
    }
}
