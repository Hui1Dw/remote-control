#ifndef RX_DISPATCHER_H
#define RX_DISPATCHER_H

#include <stdint.h>

#define RX_SRC_USART1  0
#define RX_SRC_USART2  1
#define RX_SRC_USART3  2

typedef void (*RX_Dispatcher_Handler)(uint8_t src, uint8_t byte);

void     RX_Dispatcher_Init(void);
void     RX_Dispatcher_Push(uint8_t src, uint8_t byte);
void     RX_Dispatcher_Poll(void);
void     RX_Dispatcher_RegisterHandler(RX_Dispatcher_Handler handler);

#endif