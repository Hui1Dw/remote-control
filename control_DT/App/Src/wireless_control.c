/**
 * @file wireless_control.c
 * @brief E22-1 wireless control-frame packer and USART1 sender.
 *
 * Application payload is the fixed 9-byte v2 format documented in
 * ../../无线控制帧协议.md. In E22 fixed-point mode the UART packet is
 * destination ADDH/ADDL/CHAN plus that 9-byte payload. Input values are
 * taken only from the processed, debounced public state objects. USART1 is
 * the primary E22 link: PA9/PA10 are TX/RX and PA8 (AUX) must be high before
 * a frame is submitted. USART1 TX is interrupt-driven; the static TX buffer
 * stays valid until complete.
 */
#include "wireless_control.h"

#include "button.h"
#include "joystick.h"
#include "toggle_switch.h"
#include "board.h"
#include "bsp.h"
#include "debug.h"

#define WIRELESS_CONTROL_SOF0              0xA5U
#define WIRELESS_CONTROL_VERSION           0x02U
#define WIRELESS_CONTROL_SEQUENCE_MASK     0x0FU

#define WIRELESS_CONTROL_UART_TIMEOUT_MS    20U

#define WIRELESS_CONTROL_CRC_OFFSET         8U
#define WIRELESS_CONTROL_CRC_LENGTH         7U

static uint32_t wireless_control_tx_tick;
static uint32_t wireless_control_tx_period_ms = WIRELESS_CONTROL_TX_PERIOD_DEFAULT_MS;
static uint32_t wireless_control_last_submit_tick_ms;
static uint8_t wireless_control_has_submit;
static uint8_t wireless_control_slot_due;
static uint8_t wireless_control_busy_counted;
static uint8_t wireless_control_sequence;
static uint8_t wireless_control_tx_enabled = 1U;
static uint16_t wireless_control_destination;
static uint8_t wireless_control_tx_packet[WIRELESS_E22_FIXED_HEADER_SIZE +
                                          WIRELESS_CONTROL_FRAME_SIZE];

/* CRC-8/MAXIM: init 0x00, reflected poly 0x8C. Same bytes as the receiver. */
static uint8_t WirelessControl_CalculateCrc8(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0x00U;

  while (length > 0U)
  {
    uint8_t bit_index;

    crc ^= *data;
    data++;
    length--;

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
      if ((crc & 0x01U) != 0U)
      {
        crc = (uint8_t)((crc >> 1U) ^ 0x8CU);
      }
      else
      {
        crc = (uint8_t)(crc >> 1U);
      }
    }
  }

  return crc;
}

static uint8_t WirelessControl_ClampAxis(int16_t value)
{
  if (value > 100)
  {
    value = 100;
  }
  else if (value < -100)
  {
    value = -100;
  }

  return (uint8_t)(int8_t)value;
}

static uint8_t WirelessControl_EncodeSwitch(ToggleSwitchPosition_t position)
{
  switch (position)
  {
    case TOGGLE_SWITCH_POSITION_A:
      return 0x01U;

    case TOGGLE_SWITCH_POSITION_B:
      return 0x02U;

    case TOGGLE_SWITCH_POSITION_INVALID:
    default:
      return 0x03U;
  }
}

static uint8_t WirelessControl_BuildSwitchByte(void)
{
  return (uint8_t)((WirelessControl_EncodeSwitch(debug.toggle_switch.state.switch_1) << 0U) |
                   (WirelessControl_EncodeSwitch(debug.toggle_switch.state.switch_2) << 2U) |
                   (WirelessControl_EncodeSwitch(debug.toggle_switch.state.switch_3) << 4U) |
                   (WirelessControl_EncodeSwitch(debug.toggle_switch.state.switch_4) << 6U));
}

static uint8_t WirelessControl_BuildButtonsStatusByte(void)
{
  uint8_t value = 0U;

  if (debug.button.toggle.sw1 == BUTTON_TOGGLE_ON)
  {
    value |= (1U << 0U);
  }
  if (debug.button.toggle.sw2 == BUTTON_TOGGLE_ON)
  {
    value |= (1U << 1U);
  }
  if (debug.button.toggle.sw3 == BUTTON_TOGGLE_ON)
  {
    value |= (1U << 2U);
  }
  if (debug.button.toggle.sw4 == BUTTON_TOGGLE_ON)
  {
    value |= (1U << 3U);
  }
  if (debug.joystick.calibration.valid != 0U)
  {
    value |= (1U << 4U);
  }

  return value;
}

static void WirelessControl_BuildFrame(uint8_t *frame)
{
  frame[0] = WIRELESS_CONTROL_SOF0;
  /* 高 4 位版本固定为 2；线上序号只取内部计数器的低 4 位。 */
  frame[1] = (uint8_t)((WIRELESS_CONTROL_VERSION << 4U) |
                       (wireless_control_sequence & WIRELESS_CONTROL_SEQUENCE_MASK));

  if (debug.joystick.calibration.valid != 0U)
  {
    frame[2] = WirelessControl_ClampAxis(debug.joystick.value.left_y);
    frame[3] = WirelessControl_ClampAxis(debug.joystick.value.left_x);
    frame[4] = WirelessControl_ClampAxis(debug.joystick.value.right_y);
    frame[5] = WirelessControl_ClampAxis(debug.joystick.value.right_x);
  }
  else
  {
    frame[2] = 0U;
    frame[3] = 0U;
    frame[4] = 0U;
    frame[5] = 0U;
  }

  frame[6] = WirelessControl_BuildSwitchByte();
  frame[7] = WirelessControl_BuildButtonsStatusByte();
  frame[WIRELESS_CONTROL_CRC_OFFSET] = WirelessControl_CalculateCrc8(
      &frame[1], WIRELESS_CONTROL_CRC_LENGTH);
}

static uint8_t WirelessControl_TxReady(void)
{
  return Board_E34_1_AuxReady();
}

static void WirelessControl_UpdateDebugFrame(const uint8_t *frame)
{
  uint8_t index;

  for (index = 0U; index < WIRELESS_CONTROL_FRAME_SIZE; index++)
  {
    debug.wireless.last_frame[index] = frame[index];
  }
  debug.wireless.last_frame_valid = 1U;
}

void WirelessControl_SetDestination(uint16_t address)
{
  wireless_control_destination = address;
  debug.wireless.destination_address = address;
  debug.wireless.destination_channel = WIRELESS_E22_TARGET_CHANNEL;
}

void WirelessControl_SetTxPeriodMs(uint32_t period_ms)
{
  if (period_ms == 0U)
  {
    period_ms = WIRELESS_CONTROL_TX_PERIOD_DEFAULT_MS;
  }
  wireless_control_tx_period_ms = period_ms;
  debug.wireless.diagnostics.tx_period_ms = period_ms;
}

void WirelessControl_SetTxEnabled(uint8_t enabled)
{
  wireless_control_tx_enabled = (enabled != 0U) ? 1U : 0U;
}

void WirelessControl_Init(void)
{
  wireless_control_tx_tick = Bsp_NowMs();
  wireless_control_sequence = 0U;
  debug.wireless.diagnostics.tx_success_count = 0U;
  debug.wireless.diagnostics.tx_error_count = 0U;
  debug.wireless.diagnostics.tx_busy_skip_count = 0U;
  debug.wireless.diagnostics.tx_quiet_skip_count = 0U;
  debug.wireless.diagnostics.last_tx_status = 0U;
  debug.wireless.diagnostics.tx_period_ms = wireless_control_tx_period_ms;
  debug.wireless.diagnostics.last_submit_interval_ms = 0U;
  debug.wireless.diagnostics.max_submit_interval_ms = 0U;
  wireless_control_last_submit_tick_ms = 0U;
  wireless_control_has_submit = 0U;
  wireless_control_slot_due = 0U;
  wireless_control_busy_counted = 0U;
  debug.wireless.last_frame_valid = 0U;
  debug.wireless.last_tx_size = 0U;
  debug.wireless.destination_address = wireless_control_destination;
  debug.wireless.destination_channel = WIRELESS_E22_TARGET_CHANNEL;
}

void WirelessControl_Update(void)
{
  uint8_t status;
  uint32_t now = Bsp_NowMs();

  if ((wireless_control_slot_due == 0U) &&
      ((now - wireless_control_tx_tick) >= wireless_control_tx_period_ms))
  {
    /* 10 ms 采样已到。AUX 忙时保留时隙，下一圈重试最新值，不排队、不补发。 */
    wireless_control_slot_due = 1U;
    wireless_control_busy_counted = 0U;
  }

  if (wireless_control_slot_due == 0U)
  {
    return;
  }

  if (wireless_control_tx_enabled == 0U)
  {
    debug.wireless.diagnostics.tx_quiet_skip_count++;
    /* 配对静默会拉长间隔，不计入空口占用测量。 */
    wireless_control_has_submit = 0U;
    wireless_control_slot_due = 0U;
    wireless_control_tx_tick = now;
    return;
  }

  /* AUX 低：模块还在空中发送。TxBusy：上一帧 UART IT 尚未发完。都不覆盖缓冲。 */
  if ((WirelessControl_TxReady() == 0U) || (Board_E34_1_TxBusy() != 0U))
  {
    if (wireless_control_busy_counted == 0U)
    {
      debug.wireless.diagnostics.tx_busy_skip_count++;
      wireless_control_busy_counted = 1U;
    }
    return;
  }

  /* 成功提交后才开始下一个 10 ms，避免 12 ms AUX 高却等到 20 ms。 */
  wireless_control_slot_due = 0U;
  wireless_control_tx_tick = now;

  wireless_control_tx_packet[0] = (uint8_t)(wireless_control_destination >> 8U);
  wireless_control_tx_packet[1] = (uint8_t)wireless_control_destination;
  wireless_control_tx_packet[2] = WIRELESS_E22_TARGET_CHANNEL;
  WirelessControl_BuildFrame(&wireless_control_tx_packet[WIRELESS_E22_FIXED_HEADER_SIZE]);
  WirelessControl_UpdateDebugFrame(&wireless_control_tx_packet[WIRELESS_E22_FIXED_HEADER_SIZE]);
  debug.wireless.last_tx_size = (uint8_t)sizeof(wireless_control_tx_packet);
  debug.wireless.destination_address = wireless_control_destination;
  debug.wireless.destination_channel = WIRELESS_E22_TARGET_CHANNEL;
  /* static wireless_control_tx_packet 必须保持到 USART1 TX 完成回调。 */
  status = Board_E34_1_Transmit(wireless_control_tx_packet,
                                (uint16_t)sizeof(wireless_control_tx_packet),
                                WIRELESS_CONTROL_UART_TIMEOUT_MS);
  debug.wireless.diagnostics.last_tx_status = status;

  if (status == 0U)
  {
    debug.wireless.diagnostics.tx_success_count++;
    wireless_control_sequence++;
    if (wireless_control_has_submit != 0U)
    {
      uint32_t interval_ms = now - wireless_control_last_submit_tick_ms;
      debug.wireless.diagnostics.last_submit_interval_ms = interval_ms;
      if (interval_ms > debug.wireless.diagnostics.max_submit_interval_ms)
      {
        debug.wireless.diagnostics.max_submit_interval_ms = interval_ms;
      }
    }
    wireless_control_last_submit_tick_ms = now;
    wireless_control_has_submit = 1U;
  }
  else
  {
    debug.wireless.diagnostics.tx_error_count++;
  }
}
