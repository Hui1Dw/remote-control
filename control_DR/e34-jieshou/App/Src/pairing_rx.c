#include "pairing_rx.h"

#include "board.h"
#include "debug.h"
#include "e22_provisioning.h"
#include "pairing_config.h"
#include "pairing_protocol.h"

#include <string.h>

#define PAIRING_RX_RADIO_INDEX              0U
#define PAIRING_RX_E22_CHANNEL             23U
#define PAIRING_RX_FIXED_HEADER_SIZE        3U
#define PAIRING_RX_WIRE_SIZE \
    (PAIRING_RX_FIXED_HEADER_SIZE + PAIRING_PROTOCOL_FRAME_SIZE)

#define PAIRING_RX_ACK_BEACON_PERIOD_MS   200U
#define PAIRING_RX_ACK_BEACON_DURATION_MS 2000U
#define PAIRING_RX_OK_BEACON_PERIOD_MS    100U
#define PAIRING_RX_OK_BEACON_DURATION_MS  2000U
#define PAIRING_RX_SILENCE_MS             150U
#define PAIRING_RX_SETTLE_MS              100U
#define PAIRING_RX_COMMITTED_RETRY_COUNT   10U
#define PAIRING_RX_COMMITTED_INTERVAL_MS  300U
#define PAIRING_RX_VERIFY_LISTEN_MS      8000U

#define PAIRING_RX_TXN_IDLE                 0U
#define PAIRING_RX_TXN_ACK_BEACON           1U
#define PAIRING_RX_TXN_WAIT_BEFORE_ADDR     2U
#define PAIRING_RX_TXN_WAIT_AFTER_ADDR      3U
#define PAIRING_RX_TXN_WAIT_VERIFY          4U
#define PAIRING_RX_TXN_SUCCESS              5U
#define PAIRING_RX_TXN_FAILED               6U

#define PAIRING_RX_ERR_NONE                 0U
#define PAIRING_RX_ERR_NOT_FACTORY          1U
#define PAIRING_RX_ERR_SESSION              2U
#define PAIRING_RX_ERR_E22                  3U
#define PAIRING_RX_ERR_FLASH                4U
#define PAIRING_RX_ERR_VERIFY_TIMEOUT       5U

static uint8_t pairing_factory_default;
static uint8_t identity_committed;
static uint8_t txn_state;
static uint8_t beacon_running;
static uint8_t ack_sent;
static uint8_t retries_remaining;
static uint32_t next_tx_ms;
static uint32_t deadline_ms;
static PairingProtocolFrame_t session_frame;
static uint8_t ack_payload[PAIRING_PROTOCOL_FRAME_SIZE];
static uint8_t committed_payload[PAIRING_PROTOCOL_FRAME_SIZE];
static uint8_t ok_payload[PAIRING_PROTOCOL_FRAME_SIZE];
static uint8_t wire_packet[PAIRING_RX_WIRE_SIZE];
static volatile uint32_t rx_mailbox_gen;
static uint32_t rx_mailbox_seen;
static PairingProtocolFrame_t rx_mailbox;
static uint8_t rx_mailbox_source;

static uint8_t PairingRx_TxnBusy(void)
{
  return ((txn_state == PAIRING_RX_TXN_ACK_BEACON) ||
          (txn_state == PAIRING_RX_TXN_WAIT_BEFORE_ADDR) ||
          (txn_state == PAIRING_RX_TXN_WAIT_AFTER_ADDR) ||
          (txn_state == PAIRING_RX_TXN_WAIT_VERIFY)) ? 1U : 0U;
}

static void PairingRx_SyncDebug(void)
{
  debug.pairing.txn_state = txn_state;
  debug.pairing.factory_default = pairing_factory_default;
  debug.pairing.beacon_running = beacon_running;
  debug.pairing.last_pair_number = session_frame.pair_number;
  debug.pairing.last_transmitter_address = session_frame.transmitter_address;
  debug.pairing.last_receiver_address = session_frame.receiver_address;
  debug.pairing.last_session_id = session_frame.session_id;
}

static uint8_t PairingRx_TimeReached(uint32_t now, uint32_t deadline)
{
  return ((int32_t)(now - deadline) >= 0) ? 1U : 0U;
}

static uint8_t PairingRx_FrameMatches(const PairingProtocolFrame_t *frame)
{
  return ((frame != 0) &&
          (frame->session_id == session_frame.session_id) &&
          (frame->pair_number == session_frame.pair_number) &&
          (frame->transmitter_address == session_frame.transmitter_address) &&
          (frame->receiver_address == session_frame.receiver_address)) ? 1U : 0U;
}

static uint8_t PairingRx_TakeRx(PairingProtocolFrame_t *frame, uint8_t *source)
{
  uint32_t gen;

  gen = rx_mailbox_gen;
  if (gen == rx_mailbox_seen)
  {
    return 0U;
  }

  *frame = rx_mailbox;
  *source = rx_mailbox_source;
  if (rx_mailbox_gen != gen)
  {
    return 0U;
  }

  rx_mailbox_seen = gen;
  return 1U;
}

static uint8_t PairingRx_TrySend(const uint8_t *payload, uint16_t dest)
{
  uint8_t index;

  if ((Board_RadioAuxLevel(PAIRING_RX_RADIO_INDEX) == 0U) ||
      (Board_RadioTxBusy(PAIRING_RX_RADIO_INDEX) != 0U))
  {
    debug.pairing.tx_skip_count++;
    return 0U;
  }

  wire_packet[0] = (uint8_t)(dest >> 8U);
  wire_packet[1] = (uint8_t)dest;
  wire_packet[2] = PAIRING_RX_E22_CHANNEL;
  for (index = 0U; index < PAIRING_PROTOCOL_FRAME_SIZE; index++)
  {
    wire_packet[PAIRING_RX_FIXED_HEADER_SIZE + index] = payload[index];
  }

  if (Board_RadioTransmit(PAIRING_RX_RADIO_INDEX, wire_packet,
                          PAIRING_RX_WIRE_SIZE) != 0U)
  {
    debug.pairing.tx_skip_count++;
    return 0U;
  }

  return 1U;
}

static void PairingRx_Finish(uint8_t success, uint8_t error)
{
  beacon_running = 0U;
  retries_remaining = 0U;
  if (success != 0U)
  {
    txn_state = PAIRING_RX_TXN_SUCCESS;
    debug.pairing.last_error = PAIRING_RX_ERR_NONE;
    debug.pairing.success_count++;
  }
  else
  {
    txn_state = PAIRING_RX_TXN_FAILED;
    debug.pairing.last_error = error;
    debug.pairing.fail_count++;
  }
}

static void PairingRx_UpdateIdentityWatch(const PairingConfig_t *config)
{
  pairing_factory_default = config->factory_default;
  debug.pairing.factory_default = config->factory_default;
  debug.pairing.current_pair_number = config->pair_number;
  debug.pairing.current_transmitter_address = config->transmitter_address;
  debug.pairing.current_receiver_address = config->receiver_address;
  debug.pairing.current_generation = config->generation;
  debug.pairing.reg3 = 0x43U;
}

static uint8_t PairingRx_BuildPayload(PairingProtocolCommand_t command,
                                      uint8_t *payload)
{
  PairingProtocolFrame_t frame = session_frame;

  frame.command = command;
  return PairingProtocol_Build(&frame, payload);
}

static void PairingRx_StartAckBeacon(uint32_t now)
{
  if (PairingRx_BuildPayload(PAIRING_PROTOCOL_COMMAND_PREPARE_ACK,
                             ack_payload) == 0U)
  {
    PairingRx_Finish(0U, PAIRING_RX_ERR_SESSION);
    return;
  }

  txn_state = PAIRING_RX_TXN_ACK_BEACON;
  beacon_running = 1U;
  ack_sent = 0U;
  debug.pairing.last_error = PAIRING_RX_ERR_NONE;
  next_tx_ms = now;
  deadline_ms = now + PAIRING_RX_ACK_BEACON_DURATION_MS;
}

static uint8_t PairingRx_ApplyAddress(void)
{
  PairingConfig_t committed_config;
  uint16_t previous_address;

  previous_address = debug.pairing.current_receiver_address;

  committed_config.transmitter_address = session_frame.transmitter_address;
  committed_config.receiver_address = session_frame.receiver_address;
  committed_config.pair_number = session_frame.pair_number;
  committed_config.factory_default = 0U;
  committed_config.generation = 1U;

  if (E22Provisioning_Run(session_frame.receiver_address) == 0U)
  {
    (void)E22Provisioning_Run(previous_address);
    Board_UartStartRx();
    PairingRx_Finish(0U, PAIRING_RX_ERR_E22);
    return 0U;
  }

  if (PairingConfig_Save(&committed_config) == 0U)
  {
    (void)E22Provisioning_Run(previous_address);
    Board_UartStartRx();
    PairingRx_Finish(0U, PAIRING_RX_ERR_FLASH);
    return 0U;
  }

  (void)PairingConfig_Load(&committed_config);
  committed_config.factory_default = 0U;
  PairingRx_UpdateIdentityWatch(&committed_config);
  identity_committed = 1U;
  Board_UartStartRx();
  return 1U;
}

static void PairingRx_HandlePrepare(const PairingProtocolFrame_t *frame,
                                    uint8_t source, uint32_t now)
{
  debug.pairing.prepare_count++;
  debug.pairing.last_command = (uint8_t)PAIRING_PROTOCOL_COMMAND_PREPARE;
  debug.pairing.last_source = source;

  if (PairingRx_TxnBusy() != 0U)
  {
    if (PairingRx_FrameMatches(frame) == 0U)
    {
      debug.pairing.last_error = PAIRING_RX_ERR_SESSION;
    }
    return;
  }

  if ((identity_committed != 0U) || (pairing_factory_default == 0U))
  {
    debug.pairing.last_error = PAIRING_RX_ERR_NOT_FACTORY;
    return;
  }

  session_frame = *frame;
  PairingRx_StartAckBeacon(now);
}

static void PairingRx_HandleVerify(const PairingProtocolFrame_t *frame,
                                   uint8_t source, uint32_t now)
{
  debug.pairing.verify_count++;
  debug.pairing.last_command = (uint8_t)PAIRING_PROTOCOL_COMMAND_VERIFY;
  debug.pairing.last_source = source;

  if ((txn_state != PAIRING_RX_TXN_WAIT_VERIFY) ||
      (PairingRx_FrameMatches(frame) == 0U))
  {
    return;
  }

  if (PairingRx_BuildPayload(PAIRING_PROTOCOL_COMMAND_OK, ok_payload) == 0U)
  {
    PairingRx_Finish(0U, PAIRING_RX_ERR_SESSION);
    return;
  }

  beacon_running = 1U;
  retries_remaining = 0U;
  next_tx_ms = now;
  deadline_ms = now + PAIRING_RX_OK_BEACON_DURATION_MS;
}

static void PairingRx_ProcessRx(uint32_t now)
{
  PairingProtocolFrame_t frame;
  uint8_t source;

  while (PairingRx_TakeRx(&frame, &source) != 0U)
  {
    debug.pairing.last_command = (uint8_t)frame.command;
    debug.pairing.last_source = source;

    if (frame.command == PAIRING_PROTOCOL_COMMAND_PREPARE)
    {
      PairingRx_HandlePrepare(&frame, source, now);
    }
    else if (frame.command == PAIRING_PROTOCOL_COMMAND_VERIFY)
    {
      PairingRx_HandleVerify(&frame, source, now);
    }
  }
}

void PairingRx_Init(uint8_t factory_default)
{
  pairing_factory_default = (factory_default != 0U) ? 1U : 0U;
  identity_committed = 0U;
  txn_state = PAIRING_RX_TXN_IDLE;
  beacon_running = 0U;
  ack_sent = 0U;
  retries_remaining = 0U;
  next_tx_ms = 0U;
  deadline_ms = 0U;
  rx_mailbox_gen = 0U;
  rx_mailbox_seen = 0U;
  rx_mailbox_source = 0U;
  (void)memset(&session_frame, 0, sizeof(session_frame));
  (void)memset(&rx_mailbox, 0, sizeof(rx_mailbox));
  (void)memset(ack_payload, 0, sizeof(ack_payload));
  (void)memset(committed_payload, 0, sizeof(committed_payload));
  (void)memset(ok_payload, 0, sizeof(ok_payload));
  (void)memset(wire_packet, 0, sizeof(wire_packet));
  PairingRx_SyncDebug();
}

uint8_t PairingRx_PushBuffer(uint8_t source, const uint8_t *data, uint16_t size)
{
  uint16_t offset;
  PairingProtocolFrame_t frame;
  uint8_t consumed = 0U;

  if ((data == 0) || (size == 0U) || (source >= BOARD_RADIO_COUNT))
  {
    return 0U;
  }

  for (offset = 0U; (uint16_t)(offset + PAIRING_PROTOCOL_FRAME_SIZE) <= size;
       offset++)
  {
    if (data[offset] != PAIRING_PROTOCOL_MAGIC)
    {
      continue;
    }

    consumed = 1U;
    if (PairingProtocol_Parse(&data[offset], &frame) != 0U)
    {
      rx_mailbox = frame;
      rx_mailbox_source = source;
      rx_mailbox_gen++;
      return 1U;
    }
  }

  if ((consumed == 0U) && (data[0] == PAIRING_PROTOCOL_MAGIC))
  {
    consumed = 1U;
  }

  return consumed;
}

void PairingRx_Poll(void)
{
  const uint32_t now = Board_NowMs();

  PairingRx_ProcessRx(now);

  if (txn_state == PAIRING_RX_TXN_ACK_BEACON)
  {
    if ((beacon_running != 0U) &&
        (PairingRx_TimeReached(now, next_tx_ms) != 0U))
    {
      if (PairingRx_TrySend(ack_payload,
                            PAIRING_CONFIG_DEFAULT_TX_ADDRESS) != 0U)
      {
        debug.pairing.ack_tx_count++;
        ack_sent = 1U;
        next_tx_ms = now + PAIRING_RX_ACK_BEACON_PERIOD_MS;
      }
    }

    if (PairingRx_TimeReached(now, deadline_ms) != 0U)
    {
      beacon_running = 0U;
      if (ack_sent == 0U)
      {
        PairingRx_Finish(0U, PAIRING_RX_ERR_E22);
      }
      else if (Board_RadioTxBusy(PAIRING_RX_RADIO_INDEX) == 0U)
      {
        txn_state = PAIRING_RX_TXN_WAIT_BEFORE_ADDR;
        deadline_ms = now + PAIRING_RX_SILENCE_MS;
      }
    }
  }
  else if (txn_state == PAIRING_RX_TXN_WAIT_BEFORE_ADDR)
  {
    if ((PairingRx_TimeReached(now, deadline_ms) != 0U) &&
        (Board_RadioTxBusy(PAIRING_RX_RADIO_INDEX) == 0U))
    {
      if (PairingRx_ApplyAddress() != 0U)
      {
        txn_state = PAIRING_RX_TXN_WAIT_AFTER_ADDR;
        deadline_ms = Board_NowMs() + PAIRING_RX_SETTLE_MS;
      }
    }
  }
  else if (txn_state == PAIRING_RX_TXN_WAIT_AFTER_ADDR)
  {
    if (PairingRx_TimeReached(now, deadline_ms) != 0U)
    {
      if (PairingRx_BuildPayload(PAIRING_PROTOCOL_COMMAND_COMMITTED,
                                 committed_payload) == 0U)
      {
        PairingRx_Finish(0U, PAIRING_RX_ERR_SESSION);
      }
      else
      {
        txn_state = PAIRING_RX_TXN_WAIT_VERIFY;
        retries_remaining = PAIRING_RX_COMMITTED_RETRY_COUNT;
        next_tx_ms = now;
        deadline_ms = now + PAIRING_RX_VERIFY_LISTEN_MS;
      }
    }
  }
  else if (txn_state == PAIRING_RX_TXN_WAIT_VERIFY)
  {
    if (beacon_running != 0U)
    {
      if (PairingRx_TimeReached(now, next_tx_ms) != 0U)
      {
        if (PairingRx_TrySend(ok_payload, session_frame.transmitter_address) != 0U)
        {
          debug.pairing.ok_tx_count++;
          next_tx_ms = now + PAIRING_RX_OK_BEACON_PERIOD_MS;
        }
      }

      if (PairingRx_TimeReached(now, deadline_ms) != 0U)
      {
        PairingRx_Finish(1U, PAIRING_RX_ERR_NONE);
      }
    }
    else
    {
      if ((retries_remaining > 0U) &&
          (PairingRx_TimeReached(now, next_tx_ms) != 0U))
      {
        if (PairingRx_TrySend(committed_payload,
                              PAIRING_CONFIG_DEFAULT_TX_ADDRESS) != 0U)
        {
          debug.pairing.committed_tx_count++;
          retries_remaining--;
          next_tx_ms = now + PAIRING_RX_COMMITTED_INTERVAL_MS;
        }
      }

      if (PairingRx_TimeReached(now, deadline_ms) != 0U)
      {
        /* Flash/E22 已提交，超时时不回滚身份，控制帧改走新地址。 */
        PairingRx_Finish(0U, PAIRING_RX_ERR_VERIFY_TIMEOUT);
      }
    }
  }

  PairingRx_SyncDebug();
}
