#include <stdio.h>
#include <string.h>
//#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
//#include "pico/multicore.h"
#include "KissTelemetry.h"
#include "Dshot.h"
#include "Dshot_EM32.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"


int main() 
{
    set_sys_clock_khz(FR * 1000, true);

    stdio_init_all();
    sleep_ms(1000);


    //clock_stop(clk_gpout0);// = 0, ///< Select CLK_GPOUT0 as clock source
    //clock_stop(clk_gpout1);// = 1, ///< Select CLK_GPOUT1 as clock source
    //clock_stop(clk_gpout2);// = 2, ///< Select CLK_GPOUT2 as clock source
    //clock_stop(clk_gpout3);// = 3, ///< Select CLK_GPOUT3 as clock source
    //clock_stop(clk_ref);//    = 4, ///< Select CLK_REF as clock source
    //clock_stop(clk_sys);//    = 5, ///< Select CLK_SYS as clock source
//  clock_stop(clk_peri);//   = 6, ///< Select CLK_PERI as clock source
    clock_stop(clk_usb);//    = 7, ///< Select CLK_USB as clock source
    clock_stop(clk_adc);//    = 8, ///< Select CLK_ADC as clock source
//  clock_stop(clk_rtc);//    = 9, ///< Select CLK_RTC as clock source

    //multicore_reset_core1();

    // Pin init
    gpio_init_mask(_BV(LED_RED) | _BV(LED_GREEN) | _BV(LED_BLUE) | _BV(TEST_PIN));
    gpio_set_dir_out_masked(_BV(LED_RED) | _BV(LED_GREEN) | _BV(LED_BLUE) | _BV(TEST_PIN));
    gpio_set_mask(_BV(LED_RED) | _BV(LED_GREEN) | _BV(LED_BLUE));
    //gpio_put(LED_RED, false);
    //gpio_put(LED_GREEN, false);
    //gpio_put(LED_BLUE, false);

    //while (!stdio_usb_connected()) 
    //    sleep_ms(100);

    uart_init_kiss();
    dshot_init();

    //printf("ggg - %d\n", sizeof(kiss_telem));

    kiss_send_dma();

    while (true)
    {
        __wfi();
        //gpio_xor_mask(_BV(TEST_PIN));
        KissLoop();
        dshot_pool();
    }    
}

