#pragma once

#include <stdio.h>
#include <stdint.h>
#include <pico/stdlib.h>

void erpm_init(void);

uint32_t erpm_get_frequency(void);