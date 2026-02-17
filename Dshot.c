#include "pico/stdlib.h"
#include "dshot_rx.pio.h"
#include "Dshot.h"
#include "KissTelemetry.h"
#include "Dshot_EM32.h"
#include "RingBuffer.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
PIO pio = pio0;
uint sm_rx;
bool low;
uint16_t dshot_frame;
uint8_t bit_cnt;
uint16_t cnt = 0;
volatile bool event_flags = false;
volatile uint32_t last_value;

RING_BUFFER_DEF(uint32_t, rx_buf, 32);

void __isr pio_irq_handler(void);

//////////////////////////////////////////////////////////////////////////////////////////////////////
void dshot_init(void)
{
    ring_buffer_init(&rx_buf);

    uint offset_rx = pio_add_program(pio, &dshot_rx_program);
    uint sm_rx = pio_claim_unused_sm(pio, true);
    dshot_rx_init(pio, sm_rx, offset_rx, DSHOT_PIN);    

    pio_set_irq0_source_enabled(pio, pis_sm0_rx_fifo_not_empty, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);

    /*uint offset_tx = pio_add_program(pio, &dshot_tx_program);
    uint sm_tx = pio_claim_unused_sm(pio, true);
    dshot_tx_init(pio, sm_tx, offset_tx, 27); */
}

static inline bool my_pio_sm_is_rx_fifo_empty()
{
    return (pio->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + sm_rx))) != 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
static inline uint32_t my_pio_sm_get() 
{
    return pio->rxf[sm_rx];
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void __isr pio_irq_handler(void)
{
    //gpio_xor_mask(_BV(TEST_PIN));
    while (!my_pio_sm_is_rx_fifo_empty(pio, sm_rx)) 
        ring_buffer_push_no_check(&rx_buf, -my_pio_sm_get(pio, sm_rx));
    event_flags = true;
}

#define LT  (100 * FR / 125)
#define BIT (67  * FR / 125)

//////////////////////////////////////////////////////////////////////////////////////////////////////
void dshot_pool(void)
{
    if (event_flags) 
    {
        event_flags = false;
        uint32_t ticks;
        while(ring_buffer_pop(&rx_buf, &ticks))
        {
            if(ticks > LT)
            {
                low = true;
                bit_cnt = 0;
                dshot_frame = 0;
            }
            else
            {
                if(low)
                {
                    if(ticks > BIT)
                        dshot_frame |= 0x0001;

                    if(bit_cnt < 15)
                    {
                        bit_cnt++;
                        dshot_frame <<= 1; 
                    }
                    else if(bit_cnt == 15)
                    {
                        bit_cnt++;
                        
                        uint16_t throttle = (dshot_frame >> 5) & 0x7FF;
                        uint16_t tel_req = (dshot_frame >> 4) & 0x1;

                        if(tel_req)
                        {
                            cnt = 0;
                            kiss_send_dma();
                        }
                    }
                }
                low = !low;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void dshot_tx_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = dshot_tx_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);
    //sm_config_set_jmp_pin(&c, pin);
    //sm_config_set_in_shift(&c, false, false, 32);
    //sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);    
    float clk_div = (float)clock_get_hz(clk_sys) / 100000.0f;
    sm_config_set_clkdiv(&c, clk_div);
    pio_sm_init(pio, sm, offset, &c);
    //pio_sm_clear_fifos(pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////
void dshot_rx_init(PIO pio, uint sm, uint offset, uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);

    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    pio_sm_config c = dshot_rx_program_get_default_config(offset);
    sm_config_set_jmp_pin(&c, pin);
    //sm_config_set_set_pins(&c, pin, 1);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);
    float clk_div = 1;//(float)clock_get_hz(clk_sys) / 125000000.0f;
    sm_config_set_clkdiv(&c, clk_div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}
