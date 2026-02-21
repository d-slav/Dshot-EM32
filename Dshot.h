#pragma once

#include "hardware/pio.h"

#define DSHOT_PIN  3
#define TELEMETRY_REQUEST_BIT   0x0010            

void dshot_init(void);
void dshot_pool(void);

void dshot_rx_init(PIO pio, uint sm, uint offset, uint pin);
void dshot_tx_init(PIO pio, uint sm, uint offset, uint pin);

