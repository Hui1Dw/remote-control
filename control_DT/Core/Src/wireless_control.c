/**
 * @file wireless_control.c
 * @brief E34-1 wireless control-frame packer and USART1 sender.
 *
 * Frames use the fixed 14-byte format documented in
 * ../../无线控制帧协议.md. Input values are taken only from the processed,
 * debounced public state objects. USART1 is used as the primary E34 link:
 * PA9/PA10 are TX/RX and PA8 (AUX) must be high before a frame is submitted.
 */
#include "wireless_control.h"

#include "button.h"
#include "joystick.h"
#include "toggle_switch.h"
#include "board.h"
#include "bsp.h"
#include "debug.h"

#define WIRELESS_CONTROL_SOF0              0xA5U
#define WIRELESS_CONTROL_SOF1              0x5AU
#define WIRELESS_CONTROL_PROTOCOL_VERSION  0x01U
#define WIRELESS_CONTROL_FRAME_TYPE         0x01U
#define WIRELESS_CONTROL_PAYLOAD_LENGTH     0x06U

#define WIRELESS_CONTROL_TX_PERIOD_MS       20U
#define WIRELESS_CONTROL_UART_TIMEOUT_MS    20U

#define WIRELESS_CONTROL_OFFSET_VERSION     2U
#define WIRELESS_CONTROL_OFFSET_CRC_LOW     12U
#define WIRELESS_CONTROL_CRC_INPUT_LENGTH   10U

static uint32_t wireless_control_tx_tick;
static uint8_t wireless_control_sequence;
static uint8_t wireless_control_tx_frame[WIRELESS_CONTROL_FRAME_SIZE];

static uint16_t WirelessControl_CalculateCrc16(const uint8_t *data, uint8_t length)
{
  uint16_t crc = 0xFFFFU;

  while (length > 0U)
  {
    uint8_t bit_index;

    crc ^= *data;
    data++;
    length--;

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc = (crc >> 1U) ^ 0xA001U;
      }
      else
      {
        crc >>= 1U;
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
  uint16_t crc;

  frame[0] = WIRELESS_CONTROL_SOF0;
  frame[1] = WIRELESS_CONTROL_SOF1;
  frame[2] = WIRELESS_CONTROL_PROTOCOL_VERSION;
  frame[3] = WIRELESS_CONTROL_FRAME_TYPE;
  frame[4] = wireless_control_sequence;
  frame[5] = WIRELESS_CONTROL_PAYLOAD_LENGTH;

  if (debug.joystick.calibration.valid != 0U)
  {
    frame[6] = WirelessControl_ClampAxis(debug.joystick.value.left_y);
    frame[7] = WirelessControl_ClampAxis(debug.joystick.value.left_x);
    frame[8] = WirelessControl_ClampAxis(debug.joystick.value.right_y);
    frame[9] = WirelessControl_ClampAxis(debug.joystick.value.right_x);
  }
  else
  {
    frame[6] = 0U;
    frame[7] = 0U;
    frame[8] = 0U;
    frame[9] = 0U;
  }

  frame[10] = WirelessControl_BuildSwitchByte();
  frame[11] = WirelessControl_BuildButtonsStatusByte();

  crc = WirelessControl_CalculateCrc16(&frame[WIRELESS_CONTROL_OFFSET_VERSION],
                                       WIRELESS_CONTROL_CRC_INPUT_LENGTH);
  frame[WIRELESS_CONTROL_OFFSET_CRC_LOW] = (uint8_t)(crc & 0x00FFU);
  frame[WIRELESS_CONTROL_OFFSET_CRC_LOW + 1U] = (uint8_t)(crc >> 8U);
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

void WirelessControl_Init(void)
{
  wireless_control_tx_tick = Bsp_NowMs();
  wireless_control_sequence = 0U;
  debug.wireless.diagnostics.tx_success_count = 0U;
  debug.wireless.diagnostics.tx_error_count = 0U;
  debug.wireless.diagnostics.tx_busy_skip_count = 0U;
  debug.wireless.diagnostics.last_tx_status = HAL_OK;
  debug.wireless.last_frame_valid = 0U;
}

void WirelessControl_Update(void)
{
  HAL_StatusTypeDef status;
  uint32_t now = Bsp_NowMs();

  if ((now - wireless_control_tx_tick) < WIRELESS_CONTROL_TX_PERIOD_MS)
  {
    return;
  }

  /* Do not catch up with bursts after a delayed loop or a busy module. */
  wireless_control_tx_tick = now;

  if (WirelessControl_TxReady() == 0U)
  {
    debug.wireless.diagnostics.tx_busy_skip_count++;
    return;
  }

  WirelessControl_BuildFrame(wireless_control_tx_frame);
  WirelessControl_UpdateDebugFrame(wireless_control_tx_frame);
  status = (Board_E34_1_Transmit(wireless_control_tx_frame,
                                 WIRELESS_CONTROL_FRAME_SIZE,
                                 WIRELESS_CONTROL_UART_TIMEOUT_MS) == 0U)
               ? HAL_OK
               : HAL_ERROR;
  debug.wireless.diagnostics.last_tx_status = status;

  if (status == HAL_OK)
  {
    debug.wireless.diagnostics.tx_success_count++;
    wireless_control_sequence++;
  }
  else
  {
    debug.wireless.diagnostics.tx_error_count++;
  }
}
