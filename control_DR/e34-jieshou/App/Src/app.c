#include "app.h"

#include "board.h"
#include "e22_provisioning.h"
#include "maintenance.h"
#include "receiver_control.h"

uint8_t App_Init(void)
{
  /* 上电写 E22 寄存器；内部已等待模块就绪。失败通道保持 9600，不阻止收包。 */
  (void)E22Provisioning_Run();

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
  Maintenance_Poll();
}
