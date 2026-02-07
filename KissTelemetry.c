#include "string.h"
#include "pico/time.h"
#include "pico/sync.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"

#include "KissTelemetry.h"

#define UART_ID uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_BAUD 115200
#define FRAME_GAP_US 1500

static int dma_chan;
static dma_channel_config cfg;
//kiss_telem_t kiss_telem;
ringbuf_t uart_rx;
uint8_t out_buff[sizeof(kiss_telem_t)];
critical_section_t struct_cs;
volatile bool frame_timeout = false;
volatile bool alarm = false;
#ifdef DEBUG
volatile uint64_t last_rx_time, comp_rx_time;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
static int64_t uart_tx_done_cb(alarm_id_t id, void *user_data)
{
    // přepnout pin zpět na GPIO input
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_NULL);
    gpio_set_dir(UART_TX_PIN, GPIO_IN);
    return 0; // jednorázový alarm
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static int64_t uart_tx_start_cb(alarm_id_t id, void *user_data)
{
    uint8_t buf[sizeof(kiss_telem_t)];
    critical_section_enter_blocking(&struct_cs);
    memcpy(buf, out_buff, sizeof(kiss_telem_t));
    critical_section_exit(&struct_cs);


    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_dir(UART_TX_PIN, GPIO_OUT);

    dma_channel_configure(
        dma_chan,
        &cfg,
        &uart_get_hw(UART_ID)->dr,  // WRITE: UART data register
        buf, //&kiss_telem,                        // READ: RAM buffer
        sizeof(kiss_telem_t),
        true                        // START
    );

    add_alarm_in_us(
        (sizeof(kiss_telem_t) + 1) * 10 * 1000000 / 115200,   // spočtená doba
        uart_tx_done_cb,
        NULL,
        false
    );       

    return 0; // jednorázový alarm
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int64_t frame_timeout_cb(alarm_id_t id, void *user_data)
{
#ifdef DEBUG
    comp_rx_time = time_us_64();
#endif    
    alarm = false;
    frame_timeout = true;
    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void __isr uart_rx_irq(void)
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t b = uart_getc(UART_ID);
        uint16_t next = RB_NEXT(uart_rx.head);
        if (next != uart_rx.tail) 
        {
            uart_rx.buf[uart_rx.head] = b;
            uart_rx.head = next;
        }
    }

    if(!alarm)
    {
#ifdef DEBUG        
        last_rx_time = time_us_64();
#endif        
        alarm = true;
        add_alarm_in_us(FRAME_GAP_US, frame_timeout_cb, NULL, false);        
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void uart_init_kiss_rx(void)
{
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // IRQ
    int irq = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(irq, uart_rx_irq);
    irq_set_enabled(irq, true);

    // Enable RX interrupt
    uart_set_irq_enables(UART_ID, true, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void uart_init_kiss(void)
{
    critical_section_init(&struct_cs);

    /*kiss_telem.start = 0xA5;
    kiss_telem.current = 1000;
    kiss_telem.voltage = 3000;
    kiss_telem.capacity = 4000;
    kiss_telem.rpm = 5000;
    kiss_telem.crc = CalculateCrc8((uint8_t*)&kiss_telem, sizeof(kiss_telem)-1);*/

    // Pin init
    gpio_pull_up(UART_TX_PIN);
    gpio_set_dir(UART_TX_PIN, GPIO_IN);
    gpio_put(UART_TX_PIN, true);

    // UART init
    uart_init(UART_ID, UART_BAUD);
    //gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    // DMA init
    dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);

    // UART TX DREQ
    channel_config_set_dreq(&cfg, uart_get_dreq(UART_ID, true));

    uart_init_kiss_rx();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void kiss_send_dma()
{
    add_alarm_in_us(100, uart_tx_start_cb, NULL, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t UpdateCrc8(uint8_t crc, uint8_t crc_seed)
{
    uint8_t crc_u = crc;
    crc_u ^= crc_seed;

    for (int i=0; i<8; i++)
        crc_u = ( crc_u & 0x80 ) ? 0x7 ^ ( crc_u << 1 ) : ( crc_u << 1 );

    return (crc_u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t CalculateCrc8(const uint8_t *Buf, const uint8_t BufLen)
{
    uint8_t crc = 0;
    for (int i = 0; i < BufLen; i++) {
        crc = UpdateCrc8(Buf[i], crc);
    }
    return crc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void KissLoop(void)
{
    if(frame_timeout)
    {
        // konec ramce
        frame_timeout = false;
        
        char buf[sizeof(kiss_telem_t)];

        int cnt = 0;
        while(uart_rx.head != uart_rx.tail && cnt < sizeof(kiss_telem_t))
        {
            buf[cnt++] = uart_rx.buf[uart_rx.tail];
            uart_rx.tail = RB_NEXT(uart_rx.tail);
        }

        if(cnt == sizeof(kiss_telem_t))
        {
            // prisel cely ramec
            /*kiss_telem_t *p_new_kiss_telem = (kiss_telem_t*)buf;

            printf("%d, %d, %d\n", 
                __builtin_bswap16(p_new_kiss_telem->voltage), 
                __builtin_bswap16(p_new_kiss_telem->current), 
                __builtin_bswap16(p_new_kiss_telem->capacity));*/

            critical_section_enter_blocking(&struct_cs);
            memcpy(out_buff, buf, sizeof(kiss_telem_t));
            /*kiss_telem.rpm = p_new_kiss_telem->rpm;
            kiss_telem.crc = p_new_kiss_telem->crc;
            kiss_telem.start = p_new_kiss_telem->start;
            kiss_telem.voltage = p_new_kiss_telem->voltage;
            kiss_telem.current = p_new_kiss_telem->current;
            kiss_telem.capacity = p_new_kiss_telem->capacity;*/
            critical_section_exit(&struct_cs);
        }
        else
        {
            printf("cnt = %d\n", cnt);
        }

    }
}