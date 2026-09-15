/* Unused debug bypass: not compiled. USART_c + Board now own UART RX. */
#include "e34_rx.h"
#include "rx_dispatcher.h"

UART_RX_Instance uart1_rx_inst = {0};

static void e34_frame_handler(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        RX_Dispatcher_Push(RX_SRC_USART1, data[i]);
    }
}

void E34_RX_Init(void)
{
  HAL_Delay(300);

  HAL_Delay(50);
  UART_RX_RegisterHandler(&uart1_rx_inst, e34_frame_handler);
  UART_RX_Init(&uart1_rx_inst);
}

void E34_RX_Poll(void)
{
  UART_RX_Poll(&uart1_rx_inst);
}

void E34_RX_RegisterHandler(E34_RX_FrameHandler handler)
{
  UART_RX_RegisterHandler(&uart1_rx_inst, handler);
}

uint32_t E34_RX_GetFrameCount(void)
{
  return UART_RX_GetFrameCount(&uart1_rx_inst);
}
