#include "app.h"

#include "bsp.h"
#include "button.h"
#include "e22_provisioning.h"
#include "joystick.h"
#include "toggle_switch.h"
#include "wireless_control.h"

#include "debug.h"

static uint32_t app_scan_tick;

uint8_t App_Init(void)
{
  app_scan_tick = Bsp_NowMs();

  /* Joystick_Init 仍返回 HAL 状态码：0=成功。ADC 隔离放到后续阶段。 */
  if (Joystick_Init() != 0)
  {
    return 0U;
  }

  /* 配置失败不阻止启动：失败通道保持 CubeMX 9600，可在 Watch debug.e22 看原因。 */
  (void)E22Provisioning_Run();

  WirelessControl_Init();
  if (debug.e22.ok[0] != 0U)
  {
    /* USART1 已是 115200 + 38.4k 空中，才把发送周期收到 10 ms。 */
    WirelessControl_SetTxPeriodMs(WIRELESS_CONTROL_TX_PERIOD_FAST_MS);
  }
  return 1U;
}

void App_Loop(void)
{
  if ((Bsp_NowMs() - app_scan_tick) >= 10U)
  {
    app_scan_tick += 10U;
    /* 100 Hz 扫描。发送周期由 WirelessControl 在 10/20 ms 间选择。 */
    (void)Joystick_Update();
    ToggleSwitch_Update();
    Button_Update();
  }

  WirelessControl_Update();
}
