#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "dshot_rx.pio.h"
#include "KissTelemetry.h"
//#include "AD.h"

#define DSHOT_PIN  3

void dshot_rx_init(PIO pio, uint sm, uint offset, uint pin);
void dshot_tx_init(PIO pio, uint sm, uint offset, uint pin);


int main() 
{
    stdio_init_all();

    sleep_ms(2000);

   while (!stdio_usb_connected()) 
        sleep_ms(100);

    gpio_pull_up(DSHOT_PIN);

    PIO pio = pio0;

    uint offset_rx = pio_add_program(pio, &dshot_rx_program);
    uint sm_rx = pio_claim_unused_sm(pio, true);
    dshot_rx_init(pio, sm_rx, offset_rx, DSHOT_PIN);    

    /*uint offset_tx = pio_add_program(pio, &dshot_tx_program);
    uint sm_tx = pio_claim_unused_sm(pio, true);
    dshot_tx_init(pio, sm_tx, offset_tx, 27); */

    bool low;
    uint16_t dshot_frame;
    uint8_t bit_cnt;

    uart_init_kiss();
    //adc_init_all();
    printf("ggg - %d\n", sizeof(kiss_telem));

    //adc_start();

    uint16_t cnt = 0;
    while (true)
    {
        KissLoop();
        tight_loop_contents();
        continue;

        if (!pio_sm_is_rx_fifo_empty(pio, sm_rx))
        {
            uint32_t v = pio_sm_get(pio, sm_rx);
            //uint32_t v = pio_sm_get_blocking(pio, sm_rx);
            uint32_t ticks = -v;

            if(ticks > 100)
            {
                low = true;
                bit_cnt = 0;
                dshot_frame = 0;
            }
            else
            {
                if(low)
                {
                    if(ticks > 68)
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

                        //if(cnt++ >= 100)
                        if(tel_req)
                        {
                            cnt = 0;
                            kiss_send_dma();
                            //adc_create_snapshot();//adc_start();
                            //printf("%f %f %f\n", power_snapshot.voltage_V, power_snapshot.current_A, power_snapshot.capacity_mAh/*, power_snapshot.timestamp_ms*/);
                            printf("0x%4.4X, %d, %d\n", dshot_frame, throttle, tel_req);
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
