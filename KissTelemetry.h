#pragma once

#include <stdio.h>
#include <stdint.h>
#include <pico/stdlib.h>


typedef struct __attribute__((packed)) 
{
    uint8_t  start;        // 0xA5
    uint16_t voltage;      // mV
    uint16_t current;      // mA
    uint16_t capacity;     // mAh
    uint16_t rpm;
    uint8_t  crc;
} kiss_telem_t;

extern kiss_telem_t kiss_telem;

///////////////////////////////////////////////////////////////////////////////////////////////////
#define UART_RX_BUF_SIZE 256

typedef struct {
    uint8_t buf[UART_RX_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} ringbuf_t;

extern ringbuf_t uart_rx;

#define RB_NEXT(i) (((i) + 1) % UART_RX_BUF_SIZE)

///////////////////////////////////////////////////////////////////////////////////////////////////
void KissLoop(void);
void uart_init_kiss(void);
void uart_init_kiss_rx(void);

void kiss_send_dma();
uint8_t CalculateCrc8(const uint8_t *Buf, const uint8_t BufLen);