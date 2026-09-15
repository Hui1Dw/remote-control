#include "app.h"

#include "board.h"
#include "bsp.h"
#include "receiver_control.h"

uint8_t App_Init(void)
{
  /* 等待无线模块上电稳定，沿用原 E34_RX_Init 的 300 ms + 50 ms。 */
  Bsp_DelayMs(300U);
  Bsp_DelayMs(50U);
  Board_UartStartRx();

  /* Start with a safe output until a complete, calibrated control frame arrives. */
  ReceiverControl_Init();
  Board_UartUpdateDebug();
  return 1U;
}

void App_Loop(void)
{
  Board_UartUpdateDebug();
  ReceiverControl_Poll();
}
