/* Unused debug bypass: not compiled. USART_c + Board now own UART RX. */
#ifndef UART2_RX_H
#define UART2_RX_H

#include "uart_rx.h"

typedef void (*UART2_RX_FrameHandler)(uint8_t *data, uint16_t len);

void     UART2_RX_Init(void);
void     UART2_RX_Poll(void);
void     UART2_RX_RegisterHandler(UART2_RX_FrameHandler handler);
uint32_t UART2_RX_GetFrameCount(void);
extern UART_RX_Instance uart2_rx_inst;

#endif