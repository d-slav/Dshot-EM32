#include <stdio.h>
#include <string.h>
#include "hardware/clocks.h"
#include "KissTelemetry.h"
#include "Dshot.h"

int main() 
{
    stdio_init_all();
    sleep_ms(1000);

    while (!stdio_usb_connected()) 
        sleep_ms(100);

    dshot_init();
    uart_init_kiss();

    printf("ggg - %d\n", sizeof(kiss_telem));

    while (true)
    {
        KissLoop();
        dshot_pool();
    }    
}

