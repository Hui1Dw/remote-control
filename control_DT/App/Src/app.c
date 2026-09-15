#include "app.h"

#include "bsp.h"
#include "button.h"
#include "joystick.h"
#include "toggle_switch.h"
#include "wireless_control.h"

static uint32_t app_scan_tick;

uint8_t App_Init(void)
{
  app_scan_tick = Bsp_NowMs();

  /* Joystick_Init 仍返回 HAL 状态码：0=成功。ADC 隔离放到后续阶段。 */
  if (Joystick_Init() != 0)
  {
    return 0U;
  }

  WirelessControl_Init();
  return 1U;
}

void App_Loop(void)
{
  if ((Bsp_NowMs() - app_scan_tick) >= 10U)
  {
    app_scan_tick += 10U;
    /* 100 Hz input scan. WirelessControl_Update() sends separately at 50 Hz. */
    (void)Joystick_Update();
    ToggleSwitch_Update();
    Button_Update();
  }

  WirelessControl_Update();
}
