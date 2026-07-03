#include "ERPM.h"

#define ERPM_GPIO          3
#define ERPM_TIMEOUT_US    100000      // po 100 ms bez impulsu = 0 RPM

static volatile uint32_t last_time_us = 0;
static volatile uint32_t period_us = 0;

static uint32_t period_filtered = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
void erpm_gpio_callback(uint gpio, uint32_t events)
{
    uint32_t now = time_us_32();

    if (last_time_us != 0)
    {
        uint32_t new_period = now - last_time_us;

        if (period_filtered == 0)
            period_filtered = new_period;
        else
            period_filtered = (period_filtered * 15 + new_period) / 16;

        period_us = period_filtered;
    }

    last_time_us = now;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
/*static void erpm_gpio_callback(uint gpio, uint32_t events)
{
    uint32_t now = time_us_32();

    if (last_time_us != 0)
        period_us = now - last_time_us;

    last_time_us = now;
}*/

///////////////////////////////////////////////////////////////////////////////////////////////////
void erpm_init(void)
{
    gpio_init(ERPM_GPIO);
    gpio_set_dir(ERPM_GPIO, GPIO_IN);

    // pokud je výstup ESC open collector:
    // gpio_pull_up(ERPM_GPIO);

    // pokud je push-pull:
    gpio_disable_pulls(ERPM_GPIO);

    gpio_set_irq_enabled_with_callback(
        ERPM_GPIO,
        GPIO_IRQ_EDGE_RISE,
        true,
        &erpm_gpio_callback);

    last_time_us = 0;
    period_us = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t erpm_get_period_us(void)
{
    return period_us;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t erpm_get_frequency(void)
{
    uint32_t p = period_us;

    if (p == 0)
        return 0;

    if ((time_us_32() - last_time_us) > ERPM_TIMEOUT_US)
        return 0;

    return 1000000u / p;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool erpm_is_valid(void)
{
    return (time_us_32() - last_time_us) <= ERPM_TIMEOUT_US;
}

///////////////////////////////////////////////////////////////////////////////////////////////////