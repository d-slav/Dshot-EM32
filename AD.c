#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "AD.h"
#include "pico/sync.h"

#define ADC_CHANNELS   2           // napětí + proud
#define ADC_VOLTAGE_CH 0           // GPIO26
#define ADC_CURRENT_CH 1           // GPIO27
#define ADC_SAMPLES    5           // počet vzorku na kanal

volatile power_state_t power_state;
power_state_t power_snapshot;

static int dma_chan;
static dma_channel_config cfg;
//uint16_t adc_buffer[ADC_CHANNELS * ADC_SAMPLES];
uint16_t adc_buffer[ADC_SAMPLES][ADC_CHANNELS];


critical_section_t power_cs;

repeating_timer_t adc_timer;

///////////////////////////////////////////////////////////////////////////////////////////////////
float compute_voltage(uint channel)
{
    float f = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) 
        f += adc_buffer[i][channel];
    return f / ADC_SAMPLES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void update_power_state(float voltage_raw, float current_raw)
{
    //printf("%f %f\n", voltage_raw, current_raw);

    static uint32_t last_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    float dt_h = (now - last_ms) / 3600000.0f;
    last_ms = now;

    float voltage = voltage_raw;
    float current = current_raw;
    float capacity = current * 1000.0f * dt_h;

    critical_section_enter_blocking(&power_cs);
    power_state.voltage_V = voltage;
    power_state.current_A = current;
    power_state.capacity_mAh += capacity;
    //power_state.timestamp_ms = now;
    critical_section_exit(&power_cs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void adc_create_snapshot(void)
{
    critical_section_enter_blocking(&power_cs);
    power_snapshot = power_state;
    critical_section_exit(&power_cs);    
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void __isr adc_dma_irq(void)
{
    dma_hw->ints0 = 1u << dma_chan;
    adc_run(false);   // zastav ADC

    /*for (int i = 0; i < ADC_SAMPLES; i++) 
        printf("%d %d\n", adc_buffer[i][0], adc_buffer[i][1]);
    printf("---------------------\n");*/

    update_power_state(compute_voltage(ADC_VOLTAGE_CH), compute_voltage(ADC_CURRENT_CH));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool adc_timer_callback(__unused struct repeating_timer *t) 
{
    //printf("Repeat\n");
    // ADC start
    adc_fifo_drain();
    adc_select_input(0);
    dma_channel_set_read_addr(dma_chan, &adc_hw->fifo, false);
    dma_channel_set_write_addr( dma_chan, adc_buffer, false );
    dma_channel_set_trans_count( dma_chan, ADC_CHANNELS * ADC_SAMPLES, true );    
    adc_run(true);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void adc_init_all(void)
{
    critical_section_init(&power_cs);

    adc_gpio_init(26 + ADC_VOLTAGE_CH); // ADC0
    adc_gpio_init(26 + ADC_CURRENT_CH); // ADC1
    adc_init();

    adc_set_round_robin(
        (1 << ADC_VOLTAGE_CH) |
        (1 << ADC_CURRENT_CH)
    );   

    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(0); // max speed

    // DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(dma_chan, &cfg, adc_buffer, &adc_hw->fifo, ADC_SAMPLES, true);

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, adc_dma_irq);
    irq_set_enabled(DMA_IRQ_0, true);    

    printf("Starting capture\n");
    adc_run(true);
//    dma_channel_wait_for_finish_blocking(dma_chan);
    
    add_repeating_timer_ms(-100, adc_timer_callback, NULL, &adc_timer);    
}

