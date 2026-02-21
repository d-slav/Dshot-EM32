#include "string.h"
#include "pico/time.h"
#include "pico/sync.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "Dshot_EM32.h"
#include "Dshot.h"
#include "KissTelemetry.h"

#define UART_TX_ID uart1
#define UART_TX_PIN 4
#define UART_TX_BAUD 115200

#define UART_RX_ID uart0
#define UART_RX_PIN 1
#define UART_RX_BAUD 4800
//#define FRAME_GAP_US 56000
#define FRAME_GAP_US (sizeof(kiss_telem_t) * 10 * 1000000 / 4800 + 100)

static int dma_chan;
static dma_channel_config cfg;
ringbuf_t uart_rx;
uint8_t out_buff[sizeof(kiss_telem_t)];
critical_section_t struct_cs;
volatile bool frame_timeout = false;
volatile bool alarm = false;

///////////////////////////////////////////////////////////////////////////////////////////////////
static int64_t uart_tx_done_cb(alarm_id_t id, void *user_data)
{
    // přepnout pin zpět na GPIO input
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_NULL);
    gpio_set_dir(UART_TX_PIN, GPIO_IN);
    //gpio_clr_mask(_BV(TEST_PIN3));
    return 0; // jednorázový alarm
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int64_t frame_timeout_cb(alarm_id_t id, void *user_data)
{
    //gpio_clr_mask(_BV(TEST_PIN1));
    alarm = false;
    frame_timeout = true;
    return 0; // jednorázový alarm
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void __isr uart_rx_irq(void)
{
    //gpio_set_mask(_BV(TEST_PIN1));
    while (uart_is_readable(UART_RX_ID))
    {
        uint8_t b = uart_getc(UART_RX_ID);
        uart_rx.buf[uart_rx.head] = b;
        uart_rx.head = RB_NEXT(uart_rx.head);
    }

    if(!alarm)
    {
        //gpio_set_mask(_BV(TEST_PIN1));
        alarm = true;
        add_alarm_in_us(FRAME_GAP_US, frame_timeout_cb, NULL, false);        
    }
    //gpio_clr_mask(_BV(TEST_PIN1));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void uart_init_kiss_rx(void)
{
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // UART init
    uart_init(UART_RX_ID, UART_RX_BAUD);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_fifo_enabled(UART_RX_ID, false);

    // IRQ
    int irq = UART_RX_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(irq, uart_rx_irq);
    irq_set_enabled(irq, true);

    // Enable RX interrupt
    uart_set_irq_enables(UART_RX_ID, true, false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void uart_init_kiss_tx(void)
{
    critical_section_init(&struct_cs);

    // Pin init
    gpio_pull_up(UART_TX_PIN);
    gpio_set_dir(UART_TX_PIN, GPIO_IN);
    gpio_put(UART_TX_PIN, true);

    // UART init
    uart_init(UART_TX_ID, UART_TX_BAUD);
    //gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_TX_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_TX_ID, true);

    // DMA init
    dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);

    // UART TX DREQ
    channel_config_set_dreq(&cfg, uart_get_dreq(UART_TX_ID, true));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void kiss_send_dma()
{
    uint8_t buf[sizeof(kiss_telem_t)];
    //gpio_set_mask(_BV(TEST_PIN3));
    critical_section_enter_blocking(&struct_cs);
    memcpy(buf, out_buff, sizeof(kiss_telem_t));
    critical_section_exit(&struct_cs);
    //gpio_clr_mask(_BV(TEST_PIN3));

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_dir(UART_TX_PIN, GPIO_OUT);

    dma_channel_configure(
        dma_chan, &cfg,
        &uart_get_hw(UART_TX_ID)->dr,  // WRITE: UART data register
        buf, sizeof(kiss_telem_t),
        true                        // START
    );

    add_alarm_in_us(
        sizeof(kiss_telem_t) * 10 * 1000000 / 115200 + 50,   // Len frame + 50 uS
        uart_tx_done_cb, NULL, false
    );       
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
        
        int cnt = 0;
        char buf[sizeof(kiss_telem_t)];

        while(uart_rx.head != uart_rx.tail && cnt < sizeof(kiss_telem_t))
        {
            dshot_pool();
            buf[cnt++] = uart_rx.buf[uart_rx.tail];
            uart_rx.tail = RB_NEXT(uart_rx.tail);
        }

        if(cnt == sizeof(kiss_telem_t))
        {
            // prisel cely ramec
            //gpio_set_mask(_BV(TEST_PIN1));
            critical_section_enter_blocking(&struct_cs);
            memcpy(out_buff, buf, sizeof(kiss_telem_t));
            critical_section_exit(&struct_cs);
            //gpio_clr_mask(_BV(TEST_PIN1));
        }
    }
}