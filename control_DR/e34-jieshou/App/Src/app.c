#include "app.h"

#include "board.h"
#include "debug.h"
#include "e22_provisioning.h"
#include "maintenance.h"
#include "pairing_config.h"
#include "pairing_rx.h"
#include "receiver_control.h"

static PairingConfig_t app_pairing_config;

#define PAIRING_DEBUG_FORCE_RECEIVER_FACTORY_DEFAULT 1U

static void App_ApplyPairingIdentity(void)
{
  uint8_t flash_valid = PairingConfig_Load(&app_pairing_config);

  debug.pairing.flash_valid = flash_valid;
  debug.pairing.forced_factory = 0U;

  /* DEBUG ONLY: keep RAM on the factory pair so a half-committed Flash record
   * cannot break the bench link. This does not erase the journal. */
#if PAIRING_DEBUG_FORCE_RECEIVER_FACTORY_DEFAULT
  PairingConfig_SetFactoryDefault(&app_pairing_config);
  debug.pairing.forced_factory = 1U;
#endif

  debug.pairing.factory_default = app_pairing_config.factory_default;
  debug.pairing.current_pair_number = app_pairing_config.pair_number;
  debug.pairing.current_transmitter_address = app_pairing_config.transmitter_address;
  debug.pairing.current_receiver_address = app_pairing_config.receiver_address;
  debug.pairing.current_generation = app_pairing_config.generation;
  debug.pairing.reg3 = 0x43U;
}

uint8_t App_Init(void)
{
  App_ApplyPairingIdentity();

  /* 上电写 E22 寄存器；内部已等待模块就绪。失败通道保持 9600，不阻止收包。 */
  (void)E22Provisioning_Run(app_pairing_config.receiver_address);

  /* Start with a safe output until a complete, calibrated control frame arrives. */
  ReceiverControl_Init();
  PairingRx_Init(app_pairing_config.factory_default);
  Board_UartStartRx();
  Board_UartUpdateDebug();
  return 1U;
}

void App_Loop(void)
{
  Board_UartUpdateDebug();
  ReceiverControl_Poll();
  PairingRx_Poll();
  Maintenance_Poll();
}
