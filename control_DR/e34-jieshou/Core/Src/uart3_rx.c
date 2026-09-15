#include "uart3_rx.h"
#include "rx_dispatcher.h"

UART_RX_Instance uart3_rx_inst = {0};

static void uart3_frame_handler(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        RX_Dispatcher_Push(RX_SRC_USART3, data[i]);
    }
}

void UART3_RX_Init(void)
{
  UART_RX_RegisterHandler(&uart3_rx_inst, uart3_frame_handler);
  UART_RX_Init(&uart3_rx_inst);
}

void UART3_RX_Poll(void)
{
  UART_RX_Poll(&uart3_rx_inst);
}

void UART3_RX_RegisterHandler(UART3_RX_FrameHandler handler)
{
  UART_RX_RegisterHandler(&uart3_rx_inst, handler);
}

uint32_t UART3_RX_GetFrameCount(void)
{
  return UART_RX_GetFrameCount(&uart3_rx_inst);
}