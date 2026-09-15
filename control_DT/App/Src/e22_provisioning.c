/**
 * @file e22_provisioning.c
 * @brief 上电一次性写 E22 寄存器。App 只走 Board，不碰 HAL。
 */
#include "e22_provisioning.h"

#include "board.h"
#include "bsp.h"
#include "debug.h"

#define E22_WRITE_COMMAND                 0xC0U
#define E22_READ_COMMAND                  0xC1U
#define E22_REGISTER_START                0x00U
#define E22_REGISTER_COUNT                0x09U
#define E22_COMMAND_SIZE                  12U

#define E22_ADDH                          0x00U
#define E22_ADDL                          0x00U
#define E22_NETID                         0x00U
#define E22_REG0_115200_8N1_38K4          0xE6U /* 111 00 110 */
#define E22_REG1_240B_22DBM               0x00U
#define E22_CHANNEL                       23U
#define E22_REG3_TRANSPARENT              0x00U /* 禁止 0x43 定点，那会在帧前加 ADDR+CHAN */
#define E22_CRYPT_H                       0x00U
#define E22_CRYPT_L                       0x00U

#define E22_UART_FIELD_MASK               0xF8U /* REG0 高 5 位：波特率+校验 */
#define E22_UART_115200_8N1               0xE0U /* 111 00 xxx */

#define E22_AT_TIMEOUT_MS                 100U
#define E22_AUX_TIMEOUT_MS                500U
#define E22_POWER_UP_SETTLE_MS            1000U
#define E22_BOOT_AUX_TIMEOUT_MS           2000U
#define E22_CONFIG_RETRY_COUNT            3U
#define E22_CONFIG_RETRY_GAP_MS           100U
#define E22_MODE_SETTLE_MS                100U
#define E22_WORK_BAUD                     115200U

#define E22_ERR_NONE                      0U
#define E22_ERR_AUX                       1U
#define E22_ERR_TX                        2U
#define E22_ERR_RX                        3U
#define E22_ERR_MISMATCH                  4U
#define E22_ERR_BAUD                      5U

static void E22_StoreReadback(uint8_t index, const uint8_t *reply, uint8_t rx_ok)
{
  debug.e22.readback_ok[index] = rx_ok;
  if (rx_ok == 0U)
  {
    return;
  }

  debug.e22.readback_reg0[index] = reply[6];
  debug.e22.readback_reg1[index] = reply[7];
  debug.e22.readback_reg3[index] = reply[9];
  debug.e22.readback_channel[index] = reply[8];
}

static uint8_t E22_HeaderOk(const uint8_t *reply)
{
  return ((reply[0] == E22_READ_COMMAND) && (reply[1] == E22_REGISTER_START) &&
          (reply[2] == E22_REGISTER_COUNT))
             ? 1U
             : 0U;
}

static uint8_t E22_PayloadAcceptable(const uint8_t *reply)
{
  if (E22_HeaderOk(reply) == 0U)
  {
    return 0U;
  }

  /* 230 MHz 可能改写空中速率低 3 位，UART 波特率、校验、地址、信道、REG3 必须对。 */
  if ((reply[3] != E22_ADDH) || (reply[4] != E22_ADDL) || (reply[5] != E22_NETID) ||
      ((reply[6] & E22_UART_FIELD_MASK) != E22_UART_115200_8N1) ||
      (reply[7] != E22_REG1_240B_22DBM) || (reply[8] != E22_CHANNEL) ||
      (reply[9] != E22_REG3_TRANSPARENT) || (reply[10] != E22_CRYPT_H) ||
      (reply[11] != E22_CRYPT_L))
  {
    return 0U;
  }

  return 1U;
}

static uint8_t E22_WriteRegisters(uint8_t index)
{
  uint8_t command[E22_COMMAND_SIZE] = {
      E22_WRITE_COMMAND,
      E22_REGISTER_START,
      E22_REGISTER_COUNT,
      E22_ADDH,
      E22_ADDL,
      E22_NETID,
      E22_REG0_115200_8N1_38K4,
      E22_REG1_240B_22DBM,
      E22_CHANNEL,
      E22_REG3_TRANSPARENT,
      E22_CRYPT_H,
      E22_CRYPT_L};
  uint8_t reply[E22_COMMAND_SIZE] = {0U};

  if (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_AUX;
    return 0U;
  }

  Board_RadioDrainUart(index);
  if (Board_RadioTransmitBlock(index, command, (uint16_t)sizeof(command),
                               E22_AT_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_TX;
    return 0U;
  }

  if (Board_RadioReceiveBlock(index, reply, (uint16_t)sizeof(reply),
                              E22_AT_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_RX;
    E22_StoreReadback(index, reply, 0U);
    return 0U;
  }

  E22_StoreReadback(index, reply, E22_HeaderOk(reply));
  if (E22_PayloadAcceptable(reply) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_MISMATCH;
    return 0U;
  }

  return Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS);
}

static uint8_t E22_ConfigureOne(uint8_t index)
{
  uint8_t ok;

  Board_RadioStopRx(index);
  Board_RadioSetConfigMode(index, 1U);
  Bsp_DelayMs(E22_MODE_SETTLE_MS);

  ok = E22_WriteRegisters(index);
  if ((ok != 0U) && (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U))
  {
    debug.e22.last_error[index] = E22_ERR_AUX;
    ok = 0U;
  }

  Board_RadioSetConfigMode(index, 0U);
  Bsp_DelayMs(E22_MODE_SETTLE_MS);
  if (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_AUX;
    ok = 0U;
  }

  if (ok != 0U)
  {
    /* 模块工作串口已是 115200 才改 MCU；失败则保持 CubeMX 的 9600，避免砖死。 */
    if (Board_RadioSetBaud(index, E22_WORK_BAUD) == 0U)
    {
      debug.e22.last_error[index] = E22_ERR_BAUD;
      debug.e22.mcu_baud[index] = 9600U;
      return 0U;
    }
    debug.e22.mcu_baud[index] = E22_WORK_BAUD;
    debug.e22.last_error[index] = E22_ERR_NONE;
    return 1U;
  }

  debug.e22.mcu_baud[index] = 9600U;
  return 0U;
}

uint8_t E22Provisioning_Run(void)
{
  uint8_t index;
  uint8_t all_ok = 1U;

  for (index = 0U; index < BOARD_RADIO_COUNT; index++)
  {
    debug.e22.ok[index] = 0U;
    debug.e22.readback_ok[index] = 0U;
    debug.e22.readback_reg0[index] = 0U;
    debug.e22.readback_reg1[index] = 0U;
    debug.e22.readback_reg3[index] = 0U;
    debug.e22.readback_channel[index] = 0U;
    debug.e22.attempt_count[index] = 0U;
    debug.e22.last_error[index] = E22_ERR_NONE;
    debug.e22.mcu_baud[index] = 9600U;
    Board_RadioSetConfigMode(index, 0U);
  }
  debug.e22.all_ok = 0U;

  Bsp_DelayMs(E22_POWER_UP_SETTLE_MS);

  for (index = 0U; index < BOARD_RADIO_COUNT; index++)
  {
    (void)Board_RadioWaitAuxHigh(index, E22_BOOT_AUX_TIMEOUT_MS);
  }

  for (index = 0U; index < BOARD_RADIO_COUNT; index++)
  {
    uint8_t attempt;
    uint8_t channel_ok = 0U;

    for (attempt = 0U; attempt < E22_CONFIG_RETRY_COUNT; attempt++)
    {
      debug.e22.attempt_count[index]++;
      if (E22_ConfigureOne(index) != 0U)
      {
        channel_ok = 1U;
        break;
      }
      if ((attempt + 1U) < E22_CONFIG_RETRY_COUNT)
      {
        Bsp_DelayMs(E22_CONFIG_RETRY_GAP_MS);
      }
    }

    debug.e22.ok[index] = channel_ok;
    if (channel_ok == 0U)
    {
      all_ok = 0U;
    }
  }

  debug.e22.all_ok = all_ok;
  return all_ok;
}
