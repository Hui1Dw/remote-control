#include "app.h"

#include "bsp.h"
#include "button.h"
#include "e22_provisioning.h"
#include "joystick.h"
#include "pairing_config.h"
#include "pairing_tx.h"
#include "toggle_switch.h"
#include "wireless_control.h"

#include "debug.h"

static uint32_t app_scan_tick;
static PairingConfig_t app_pairing_config;

#define PAIRING_DEBUG_FORCE_TRANSMITTER_FACTORY_DEFAULT 1U

static void App_ApplyPairingIdentity(void)
{
  uint8_t flash_valid = PairingConfig_Load(&app_pairing_config);

  debug.pairing.flash_valid = flash_valid;
  debug.pairing.forced_factory = 0U;

  /* DEBUG ONLY: keep RAM on the factory pair so a half-committed Flash record
   * cannot break the bench link. This does not erase the journal. */
#if PAIRING_DEBUG_FORCE_TRANSMITTER_FACTORY_DEFAULT
  PairingConfig_SetFactoryDefault(&app_pairing_config);
  debug.pairing.forced_factory = 1U;
#endif

  debug.pairing.factory_default = app_pairing_config.factory_default;
  debug.pairing.current_pair_number = app_pairing_config.pair_number;
  debug.pairing.current_transmitter_address = app_pairing_config.transmitter_address;
  debug.pairing.current_receiver_address = app_pairing_config.receiver_address;
  debug.pairing.current_generation = app_pairing_config.generation;
  debug.pairing.reg3 = 0x43U;

  WirelessControl_SetDestination(app_pairing_config.receiver_address);
}

uint8_t App_Init(void)
{
  app_scan_tick = Bsp_NowMs();

  /* Joystick_Init 返回 0=成功。ADC 经 Board/BSP 轮询，不暴露 HAL 类型。 */
  if (Joystick_Init() != 0U)
  {
    return 0U;
  }

  App_ApplyPairingIdentity();

  /* 配置失败不阻止启动：失败通道保持 CubeMX 9600，可在 Watch debug.e22 看原因。 */
  (void)E22Provisioning_Run(app_pairing_config.transmitter_address);

  WirelessControl_Init();
  if (debug.e22.ok[0] != 0U)
  {
    /* USART1 已是 115200 + 38.4k 空中，才把发送周期收到 10 ms。 */
    WirelessControl_SetTxPeriodMs(WIRELESS_CONTROL_TX_PERIOD_FAST_MS);
  }
  PairingTx_Init();
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
    PairingTx_Update();
  }

  WirelessControl_Update();
  PairingTx_Poll();
}
