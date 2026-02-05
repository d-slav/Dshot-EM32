#pragma once

typedef struct {
    float voltage_V;      // okamžité napětí
    float current_A;      // okamžitý proud
    float capacity_mAh;   // integrovaná kapacita
    //uint32_t timestamp_ms;
} power_state_t;

extern power_state_t power_snapshot;

void adc_init_all(void);
void adc_create_snapshot(void);
