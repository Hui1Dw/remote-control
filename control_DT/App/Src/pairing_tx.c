#include "pairing_tx.h"

#include "board.h"
#include "button.h"
#include "debug.h"
#include "e22_provisioning.h"
#include "pairing_config.h"
#include "pairing_input.h"
#include "pairing_protocol.h"
#include "wireless_control.h"

#include <string.h>

#define PAIRING_TX_PREPARE_RETRY_COUNT        3U
#define PAIRING_TX_VERIFY_RETRY_COUNT         3U
#define PAIRING_TX_RETRY_INTERVAL_MS        300U
#define PAIRING_TX_ACK_LISTEN_MS           8000U
#define PAIRING_TX_COMMITTED_LISTEN_MS     8000U
#define PAIRING_TX_VERIFY_LISTEN_MS        8000U
#define PAIRING_TX_E22_CHANNEL               23U
#define PAIRING_TX_WIRE_SIZE \
    (WIRELESS_E22_FIXED_HEADER_SIZE + PAIRING_PROTOCOL_FRAME_SIZE)

#define PAIRING_TX_TXN_IDLE                   0U
#define PAIRING_TX_TXN_PREPARE_TX             1U
#define PAIRING_TX_TXN_WAIT_COMMITTED         2U
#define PAIRING_TX_TXN_WAIT_VERIFY            3U
#define PAIRING_TX_TXN_SUCCESS                4U
#define PAIRING_TX_TXN_FAILED                 5U

#define PAIRING_TX_ERR_NONE                   0U
#define PAIRING_TX_ERR_NOT_FACTORY            1U
#define PAIRING_TX_ERR_BUILD                  2U
#define PAIRING_TX_ERR_ACK_TIMEOUT            3U
#define PAIRING_TX_ERR_COMMITTED_TIMEOUT      4U
#define PAIRING_TX_ERR_VERIFY_TIMEOUT         5U
#define PAIRING_TX_ERR_E22                    6U
#define PAIRING_TX_ERR_FLASH                  7U

static PairingInput_t pairing_input;
static uint8_t pairing_capture;
static uint8_t pairing_enter_pending;
static uint8_t last_sw3_physical;
static uint8_t last_sw3_valid;
static uint8_t pairing_factory_default;
static uint8_t txn_state;
static uint8_t retries_remaining;
static uint32_t next_tx_ms;
static uint32_t deadline_ms;
static PairingProtocolFrame_t prepare_frame;
static uint8_t prepare_payload[PAIRING_PROTOCOL_FRAME_SIZE];
static uint8_t verify_payload[PAIRING_PROTOCOL_FRAME_SIZE];
static uint8_t wire_packet[PAIRING_TX_WIRE_SIZE];
static volatile uint32_t rx_mailbox_gen;
static uint32_t rx_mailbox_seen;
static PairingProtocolFrame_t rx_mailbox;

static uint8_t PairingTx_TxnBusy(void)
{
  return ((txn_state == PAIRING_TX_TXN_PREPARE_TX) ||
          (txn_state == PAIRING_TX_TXN_WAIT_COMMITTED) ||
          (txn_state == PAIRING_TX_TXN_WAIT_VERIFY)) ? 1U : 0U;
}

static void PairingTx_SetQuiet(uint8_t quiet)
{
  WirelessControl_SetTxEnabled((quiet != 0U) ? 0U : 1U);
}

static void PairingTx_StopCapture(void)
{
  pairing_capture = 0U;
  pairing_enter_pending = 0U;
  Button_SetSw3Capture(0U);
  if (PairingTx_TxnBusy() == 0U)
  {
    PairingTx_SetQuiet(0U);
  }
}

static void PairingTx_SyncDebug(void)
{
  debug.pairing.capture_active = pairing_capture;
  debug.pairing.enter_pending = pairing_enter_pending;
  debug.pairing.state = (uint8_t)pairing_input.state;
  debug.pairing.last_cancel_reason = (uint8_t)pairing_input.last_cancel_reason;
  debug.pairing.first_number = pairing_input.first_number;
  debug.pairing.second_number = pairing_input.second_number;
  debug.pairing.request_ready = pairing_input.request_ready;
  debug.pairing.requested_pair_number = pairing_input.requested_pair_number;
  debug.pairing.start_count = pairing_input.start_count;
  debug.pairing.short_press_count = pairing_input.short_press_count;
  debug.pairing.confirm_count = pairing_input.confirm_count;
  debug.pairing.cancel_count = pairing_input.cancel_count;
  debug.pairing.timeout_count = pairing_input.timeout_count;
  debug.pairing.overflow_count = pairing_input.overflow_count;
  debug.pairing.txn_state = txn_state;
  debug.pairing.retries_remaining = retries_remaining;
  debug.pairing.factory_default = pairing_factory_default;
  debug.pairing.tx_quiet =
      ((pairing_capture != 0U) || (PairingTx_TxnBusy() != 0U)) ? 1U : 0U;
}

static uint8_t PairingTx_TimeReached(uint32_t now, uint32_t deadline)
{
  return ((int32_t)(now - deadline) >= 0) ? 1U : 0U;
}

static uint8_t PairingTx_FrameMatches(const PairingProtocolFrame_t *frame)
{
  return ((frame != 0) &&
          (frame->session_id == prepare_frame.session_id) &&
          (frame->pair_number == prepare_frame.pair_number) &&
          (frame->transmitter_address == prepare_frame.transmitter_address) &&
          (frame->receiver_address == prepare_frame.receiver_address)) ? 1U : 0U;
}

static void PairingTx_OnIdleRx(const uint8_t *data, uint16_t size)
{
  uint16_t offset;
  PairingProtocolFrame_t frame;

  if ((data == 0) || (size < PAIRING_PROTOCOL_FRAME_SIZE))
  {
    return;
  }

  for (offset = 0U; (uint16_t)(offset + PAIRING_PROTOCOL_FRAME_SIZE) <= size;
       offset++)
  {
    if (data[offset] != PAIRING_PROTOCOL_MAGIC)
    {
      continue;
    }
    if (PairingProtocol_Parse(&data[offset], &frame) != 0U)
    {
      rx_mailbox = frame;
      rx_mailbox_gen++;
      return;
    }
  }
}

static uint8_t PairingTx_TakeRx(PairingProtocolFrame_t *frame)
{
  uint32_t gen;

  gen = rx_mailbox_gen;
  if (gen == rx_mailbox_seen)
  {
    return 0U;
  }

  *frame = rx_mailbox;
  if (rx_mailbox_gen != gen)
  {
    return 0U;
  }

  rx_mailbox_seen = gen;
  return 1U;
}

static uint8_t PairingTx_TrySend(const uint8_t *payload, uint16_t dest)
{
  uint8_t index;

  if ((Board_E34_1_AuxReady() == 0U) || (Board_E34_1_TxBusy() != 0U))
  {
    debug.pairing.tx_skip_count++;
    return 0U;
  }

  wire_packet[0] = (uint8_t)(dest >> 8U);
  wire_packet[1] = (uint8_t)dest;
  wire_packet[2] = PAIRING_TX_E22_CHANNEL;
  for (index = 0U; index < PAIRING_PROTOCOL_FRAME_SIZE; index++)
  {
    wire_packet[WIRELESS_E22_FIXED_HEADER_SIZE + index] = payload[index];
  }

  if (Board_E34_1_Transmit(wire_packet, PAIRING_TX_WIRE_SIZE, 20U) != 0U)
  {
    debug.pairing.tx_skip_count++;
    return 0U;
  }

  return 1U;
}

static void PairingTx_StopIdleRx(void)
{
  Board_RadioStopIdleRx(0U);
  debug.pairing.idle_rx_ok = 0U;
}

static uint8_t PairingTx_StartIdleRx(void)
{
  debug.pairing.idle_rx_ok = Board_RadioStartIdleRx(0U, PairingTx_OnIdleRx);
  return debug.pairing.idle_rx_ok;
}

static void PairingTx_Finish(uint8_t success, uint8_t error)
{
  PairingTx_StopIdleRx();
  retries_remaining = 0U;
  if (success != 0U)
  {
    txn_state = PAIRING_TX_TXN_SUCCESS;
    debug.pairing.last_error = PAIRING_TX_ERR_NONE;
    debug.pairing.success_count++;
  }
  else
  {
    txn_state = PAIRING_TX_TXN_FAILED;
    debug.pairing.last_error = error;
    debug.pairing.fail_count++;
  }
  PairingTx_StopCapture();
  PairingTx_SetQuiet(0U);
}

static void PairingTx_UpdateIdentityWatch(const PairingConfig_t *config)
{
  pairing_factory_default = config->factory_default;
  debug.pairing.factory_default = config->factory_default;
  debug.pairing.current_pair_number = config->pair_number;
  debug.pairing.current_transmitter_address = config->transmitter_address;
  debug.pairing.current_receiver_address = config->receiver_address;
  debug.pairing.current_generation = config->generation;
  debug.pairing.reg3 = 0x43U;
  WirelessControl_SetDestination(config->receiver_address);
}

static void PairingTx_HandleCommitted(void)
{
  PairingConfig_t committed_config;
  PairingConfig_t previous_config;
  uint16_t previous_address;
  uint8_t index;

  previous_address = debug.pairing.current_transmitter_address;
  previous_config.transmitter_address = previous_address;
  previous_config.receiver_address = debug.pairing.current_receiver_address;
  previous_config.pair_number = debug.pairing.current_pair_number;
  previous_config.factory_default = pairing_factory_default;
  previous_config.generation = debug.pairing.current_generation;

  PairingTx_StopIdleRx();

  committed_config.transmitter_address = prepare_frame.transmitter_address;
  committed_config.receiver_address = prepare_frame.receiver_address;
  committed_config.pair_number = prepare_frame.pair_number;
  committed_config.factory_default = 0U;
  committed_config.generation = 1U;

  if (E22Provisioning_Run(prepare_frame.transmitter_address) == 0U)
  {
    (void)E22Provisioning_Run(previous_address);
    PairingTx_Finish(0U, PAIRING_TX_ERR_E22);
    return;
  }

  if (PairingConfig_Save(&committed_config) == 0U)
  {
    (void)E22Provisioning_Run(previous_address);
    PairingTx_Finish(0U, PAIRING_TX_ERR_FLASH);
    return;
  }

  (void)PairingConfig_Load(&committed_config);
  committed_config.factory_default = 0U;
  PairingTx_UpdateIdentityWatch(&committed_config);

  prepare_frame.command = PAIRING_PROTOCOL_COMMAND_VERIFY;
  if (PairingProtocol_Build(&prepare_frame, verify_payload) == 0U)
  {
    PairingTx_Finish(0U, PAIRING_TX_ERR_BUILD);
    return;
  }
  for (index = 0U; index < PAIRING_PROTOCOL_FRAME_SIZE; index++)
  {
    (void)previous_config.pair_number;
  }

  txn_state = PAIRING_TX_TXN_WAIT_VERIFY;
  retries_remaining = PAIRING_TX_VERIFY_RETRY_COUNT;
  next_tx_ms = Board_NowMs();
  deadline_ms = next_tx_ms + PAIRING_TX_VERIFY_LISTEN_MS;
  (void)PairingTx_StartIdleRx();
}

static void PairingTx_ProcessRx(void)
{
  PairingProtocolFrame_t frame;

  while (PairingTx_TakeRx(&frame) != 0U)
  {
    debug.pairing.last_rx_command = (uint8_t)frame.command;
    if (PairingTx_FrameMatches(&frame) == 0U)
    {
      continue;
    }

    if ((txn_state == PAIRING_TX_TXN_PREPARE_TX) &&
        (frame.command == PAIRING_PROTOCOL_COMMAND_PREPARE_ACK))
    {
      debug.pairing.ack_rx_count++;
      txn_state = PAIRING_TX_TXN_WAIT_COMMITTED;
      retries_remaining = 0U;
      deadline_ms = Board_NowMs() + PAIRING_TX_COMMITTED_LISTEN_MS;
    }
    else if ((txn_state == PAIRING_TX_TXN_WAIT_COMMITTED) &&
             (frame.command == PAIRING_PROTOCOL_COMMAND_COMMITTED))
    {
      debug.pairing.committed_rx_count++;
      PairingTx_HandleCommitted();
      return;
    }
    else if ((txn_state == PAIRING_TX_TXN_WAIT_VERIFY) &&
             (frame.command == PAIRING_PROTOCOL_COMMAND_OK))
    {
      debug.pairing.ok_rx_count++;
      PairingTx_Finish(1U, PAIRING_TX_ERR_NONE);
      return;
    }
  }
}

static void PairingTx_StartTransaction(uint32_t now)
{
  txn_state = PAIRING_TX_TXN_PREPARE_TX;
  debug.pairing.last_error = PAIRING_TX_ERR_NONE;
  debug.pairing.last_rx_command = 0U;
  retries_remaining = PAIRING_TX_PREPARE_RETRY_COUNT;
  next_tx_ms = now;
  deadline_ms = now + PAIRING_TX_ACK_LISTEN_MS;
  PairingTx_SetQuiet(1U);
  (void)PairingTx_StartIdleRx();
}

static void PairingTx_CaptureConfirmedNumber(uint32_t now)
{
  uint8_t pair_number;
  uint16_t transmitter_address;
  uint16_t receiver_address;
  PairingProtocolFrame_t frame;
  uint8_t payload[PAIRING_PROTOCOL_FRAME_SIZE];
  uint8_t index;

  if (PairingInput_TakeConfirmedNumber(&pairing_input, &pair_number) == 0U)
  {
    return;
  }

  debug.pairing.last_confirmed_pair_number = pair_number;
  debug.pairing.last_session_id = (now != 0U) ? now : 1U;
  debug.pairing.last_prepare_valid = 0U;
  debug.pairing.last_transmitter_address = 0U;
  debug.pairing.last_receiver_address = 0U;
  for (index = 0U; index < PAIRING_PROTOCOL_FRAME_SIZE; index++)
  {
    debug.pairing.last_prepare[index] = 0U;
  }

  if (PairingTx_TxnBusy() != 0U)
  {
    return;
  }

  if (pairing_factory_default == 0U)
  {
    debug.pairing.last_prepare_build_ok = 0U;
    PairingTx_Finish(0U, PAIRING_TX_ERR_NOT_FACTORY);
    return;
  }

  if (PairingConfig_Derive(pair_number, &transmitter_address,
                           &receiver_address) == 0U)
  {
    debug.pairing.last_prepare_build_ok = 0U;
    PairingTx_Finish(0U, PAIRING_TX_ERR_BUILD);
    return;
  }

  debug.pairing.last_transmitter_address = transmitter_address;
  debug.pairing.last_receiver_address = receiver_address;

  frame.command = PAIRING_PROTOCOL_COMMAND_PREPARE;
  frame.session_id = debug.pairing.last_session_id;
  frame.pair_number = pair_number;
  frame.transmitter_address = transmitter_address;
  frame.receiver_address = receiver_address;
  if (PairingProtocol_Build(&frame, payload) == 0U)
  {
    debug.pairing.last_prepare_build_ok = 0U;
    PairingTx_Finish(0U, PAIRING_TX_ERR_BUILD);
    return;
  }

  for (index = 0U; index < PAIRING_PROTOCOL_FRAME_SIZE; index++)
  {
    debug.pairing.last_prepare[index] = payload[index];
    prepare_payload[index] = payload[index];
  }
  debug.pairing.last_prepare_valid = 1U;
  debug.pairing.last_prepare_build_ok = 1U;
  prepare_frame = frame;
  PairingTx_StartTransaction(now);
}

void PairingTx_Init(void)
{
  PairingInput_Init(&pairing_input, Board_NowMs());
  pairing_capture = 0U;
  pairing_enter_pending = 0U;
  last_sw3_physical = (uint8_t)BUTTON_RELEASED;
  last_sw3_valid = 0U;
  pairing_factory_default = debug.pairing.factory_default;
  txn_state = PAIRING_TX_TXN_IDLE;
  retries_remaining = 0U;
  next_tx_ms = 0U;
  deadline_ms = 0U;
  rx_mailbox_gen = 0U;
  rx_mailbox_seen = 0U;
  (void)memset(&prepare_frame, 0, sizeof(prepare_frame));
  (void)memset(&rx_mailbox, 0, sizeof(rx_mailbox));
  (void)memset(prepare_payload, 0, sizeof(prepare_payload));
  (void)memset(verify_payload, 0, sizeof(verify_payload));
  (void)memset(wire_packet, 0, sizeof(wire_packet));
  Button_SetSw3Capture(0U);
  PairingTx_SetQuiet(0U);
  PairingTx_SyncDebug();
}

void PairingTx_Update(void)
{
  const uint32_t now = Board_NowMs();
  const uint8_t sw3 = (uint8_t)debug.button.physical.sw3;

  if (PairingTx_TxnBusy() != 0U)
  {
    if (debug.button.sw3_pair_enter != 0U)
    {
      debug.button.sw3_pair_enter = 0U;
    }
    last_sw3_physical = sw3;
    last_sw3_valid = 1U;
    PairingTx_SyncDebug();
    return;
  }

  if (pairing_capture == 0U)
  {
    if (debug.button.sw3_pair_enter != 0U)
    {
      debug.button.sw3_pair_enter = 0U;
      pairing_capture = 1U;
      pairing_enter_pending = 1U;
      Button_SetSw3Capture(1U);
      PairingTx_SetQuiet(1U);
      if (sw3 == (uint8_t)BUTTON_RELEASED)
      {
        pairing_enter_pending = 0U;
        PairingInput_Enter(&pairing_input, now);
      }
    }
  }
  else
  {
    PairingInput_Process(&pairing_input, now);

    if ((last_sw3_valid != 0U) &&
        (last_sw3_physical == (uint8_t)BUTTON_RELEASED) &&
        (sw3 == (uint8_t)BUTTON_PRESSED))
    {
      if (pairing_enter_pending == 0U)
      {
        PairingInput_OnPressed(&pairing_input, now);
      }
    }
    else if ((last_sw3_valid != 0U) &&
             (last_sw3_physical == (uint8_t)BUTTON_PRESSED) &&
             (sw3 == (uint8_t)BUTTON_RELEASED))
    {
      if (pairing_enter_pending != 0U)
      {
        pairing_enter_pending = 0U;
        PairingInput_Enter(&pairing_input, now);
      }
      else
      {
        PairingInput_OnReleased(&pairing_input, now);
        PairingTx_CaptureConfirmedNumber(now);
      }
    }

    if ((pairing_capture != 0U) &&
        (pairing_enter_pending == 0U) &&
        (pairing_input.state == PAIRING_INPUT_STATE_IDLE) &&
        (pairing_input.request_ready == 0U) &&
        (PairingTx_TxnBusy() == 0U))
    {
      PairingTx_StopCapture();
    }
  }

  last_sw3_physical = sw3;
  last_sw3_valid = 1U;
  PairingTx_SyncDebug();
}

void PairingTx_Poll(void)
{
  const uint32_t now = Board_NowMs();

  if (PairingTx_TxnBusy() == 0U)
  {
    return;
  }

  PairingTx_ProcessRx();
  if (PairingTx_TxnBusy() == 0U)
  {
    PairingTx_SyncDebug();
    return;
  }

  if (txn_state == PAIRING_TX_TXN_PREPARE_TX)
  {
    if ((retries_remaining > 0U) &&
        (PairingTx_TimeReached(now, next_tx_ms) != 0U))
    {
      if (PairingTx_TrySend(prepare_payload,
                            PAIRING_CONFIG_DEFAULT_RX_ADDRESS) != 0U)
      {
        debug.pairing.prepare_tx_count++;
        retries_remaining--;
        next_tx_ms = now + PAIRING_TX_RETRY_INTERVAL_MS;
        deadline_ms = now + PAIRING_TX_ACK_LISTEN_MS;
      }
    }

    if (PairingTx_TimeReached(now, deadline_ms) != 0U)
    {
      PairingTx_Finish(0U, PAIRING_TX_ERR_ACK_TIMEOUT);
    }
  }
  else if (txn_state == PAIRING_TX_TXN_WAIT_COMMITTED)
  {
    if (PairingTx_TimeReached(now, deadline_ms) != 0U)
    {
      PairingTx_Finish(0U, PAIRING_TX_ERR_COMMITTED_TIMEOUT);
    }
  }
  else if (txn_state == PAIRING_TX_TXN_WAIT_VERIFY)
  {
    if ((retries_remaining > 0U) &&
        (PairingTx_TimeReached(now, next_tx_ms) != 0U))
    {
      if (PairingTx_TrySend(verify_payload,
                            prepare_frame.receiver_address) != 0U)
      {
        debug.pairing.verify_tx_count++;
        retries_remaining--;
        next_tx_ms = now + PAIRING_TX_RETRY_INTERVAL_MS;
        deadline_ms = now + PAIRING_TX_VERIFY_LISTEN_MS;
      }
    }

    if (PairingTx_TimeReached(now, deadline_ms) != 0U)
    {
      /* Flash/E22 已提交，超时时不回滚身份，控制帧改走新地址。 */
      PairingTx_Finish(0U, PAIRING_TX_ERR_VERIFY_TIMEOUT);
    }
  }

  PairingTx_SyncDebug();
}
