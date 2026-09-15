/* Unused debug bypass: not compiled. USART_c + Board now own UART RX. */
#include "uart2_rx.h"
#include "rx_dispatcher.h"

UART_RX_Instance uart2_rx_inst = {0};

static void uart2_frame_handler(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        RX_Dispatcher_Push(RX_SRC_USART2, data[i]);
    }
}

void UART2_RX_Init(void)
{
  UART_RX_RegisterHandler(&uart2_rx_inst, uart2_frame_handler);
  UART_RX_Init(&uart2_rx_inst);
}

void UART2_RX_Poll(void)
{
  UART_RX_Poll(&uart2_rx_inst);
}

void UART2_RX_RegisterHandler(UART2_RX_FrameHandler handler)
{
  UART_RX_RegisterHandler(&uart2_rx_inst, handler);
}

uint32_t UART2_RX_GetFrameCount(void)
{
  return UART_RX_GetFrameCount(&uart2_rx_inst);
}