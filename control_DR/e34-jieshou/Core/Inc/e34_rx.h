#ifndef E34_RX_H
#define E34_RX_H

#include "uart_rx.h"

typedef void (*E34_RX_FrameHandler)(uint8_t *data, uint16_t len);

void     E34_RX_Init(void);
void     E34_RX_Poll(void);
void     E34_RX_RegisterHandler(E34_RX_FrameHandler handler);
uint32_t E34_RX_GetFrameCount(void);
extern UART_RX_Instance uart1_rx_inst;

#endif