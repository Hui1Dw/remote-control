#include "app.h"

#include "board.h"
#include "debug.h"
#include "e34_rx.h"
#include "receiver_control.h"
#include "rx_dispatcher.h"
#include "uart2_rx.h"
#include "uart3_rx.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

/*
 * USART1/2/3 remain connected to the three redundant E34 modules.
 * ReceiverControl keeps one protocol parser per UART, so bytes from different
 * radio links can never be combined into a false control frame.
 *
 * UART 句柄绑定仍留在 App：阶段 1 后半段由 USART_c 接管 Receive-to-IDLE
 * 之前，不能把 HAL_UARTEx_RxEventCallback 从 uart_rx.c 抢走。
 */
static void Wireless_Aux_Update(void)
{
  /* PA4 is the AUX pin of the USART2 E34 module. */
  debug.aux_usart2 = Board_Usart2_AuxReady();
  /* PB3 is now USART2_TX.  The AUX pin for the USART1 module was not supplied. */
  debug.aux_usart1 = 0U;
}

static void rx_dispatch_handler(uint8_t src, uint8_t byte)
{
  if (src > RX_SRC_USART3)
  {
    return;
  }

  debug.uart[src].rx_last = byte;
  debug.uart[src].rx_count++;
  ReceiverControl_PushByte(src, byte);
}

uint8_t App_Init(void)
{
  RX_Dispatcher_Init();
  RX_Dispatcher_RegisterHandler(rx_dispatch_handler);

  uart1_rx_inst.huart = &huart1;
  E34_RX_Init();

  uart2_rx_inst.huart = &huart2;
  UART2_RX_Init();

  uart3_rx_inst.huart = &huart3;
  UART3_RX_Init();

  /* Start with a safe output until a complete, calibrated control frame arrives. */
  ReceiverControl_Init();
  Wireless_Aux_Update();
  return 1U;
}

void App_Loop(void)
{
  E34_RX_Poll();
  UART2_RX_Poll();
  UART3_RX_Poll();
  RX_Dispatcher_Poll();
  Wireless_Aux_Update();
  ReceiverControl_Poll();
}
