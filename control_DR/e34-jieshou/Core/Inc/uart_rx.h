#ifndef UART_RX_H
#define UART_RX_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define UART_RX_BUF_SIZE 256

typedef void (*UART_RX_FrameHandler)(uint8_t *data, uint16_t len);

typedef struct {
    UART_HandleTypeDef  *huart;
    uint8_t              rx_buf[UART_RX_BUF_SIZE];
    volatile uint16_t    rx_len;
    volatile uint8_t     rx_ready;
    volatile uint32_t    rx_frame_count;
    volatile uint8_t     first_frame_done;
    UART_RX_FrameHandler frame_handler;
} UART_RX_Instance;

void     UART_RX_Init(UART_RX_Instance *inst);
void     UART_RX_Poll(UART_RX_Instance *inst);
void     UART_RX_RegisterHandler(UART_RX_Instance *inst, UART_RX_FrameHandler handler);
uint32_t UART_RX_GetFrameCount(UART_RX_Instance *inst);
void     UART_RX_StartDMA(UART_RX_Instance *inst);


#endif