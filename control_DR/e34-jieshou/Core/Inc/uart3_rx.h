/* Unused debug bypass: not compiled. USART_c + Board now own UART RX. */
#ifndef UART3_RX_H
#define UART3_RX_H

#include "uart_rx.h"

typedef void (*UART3_RX_FrameHandler)(uint8_t *data, uint16_t len);

void     UART3_RX_Init(void);
void     UART3_RX_Poll(void);
void     UART3_RX_RegisterHandler(UART3_RX_FrameHandler handler);
uint32_t UART3_RX_GetFrameCount(void);
extern UART_RX_Instance uart3_rx_inst;

#endif