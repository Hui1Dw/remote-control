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
#define E22_READ_COMMAND_SIZE             3U

#define E22_NETID                         0x00U
#define E22_REG0_115200_8N1_62K5          0xE7U /* 111 00 111 */
#define E22_REG1_240B_22DBM               0x00U
#define E22_CHANNEL                       23U
#define E22_REG3_FIXED_POINT              0x43U /* 定点：UART 前 3 字节是目标 ADDH/ADDL/CHAN */
#define E22_CRYPT_H                       0x00U
#define E22_CRYPT_L                       0x00U

#define E22_UART_FIELD_MASK               0xF8U /* REG0 高 5 位：波特率+校验 */
#define E22_UART_115200_8N1               0xE0U /* 111 00 xxx */

#define E22_AT_TIMEOUT_MS                 100U
#define E22_C1_FALLBACK_TIMEOUT_MS        1000U
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

#define E22_STEP_NONE                     0U
#define E22_STEP_PRE_WRITE_AUX            1U
#define E22_STEP_TX                       2U
#define E22_STEP_RX                       3U
#define E22_STEP_MISMATCH                 4U
#define E22_STEP_POST_WRITE_AUX           5U
#define E22_STEP_CONFIG_AUX               6U
#define E22_STEP_WORK_MODE_AUX            7U
#define E22_STEP_SET_BAUD                 8U
#define E22_STEP_C1_TX                    9U
#define E22_STEP_C1_RX                    10U
#define E22_STEP_C1_MISMATCH              11U

static uint16_t e22_local_address;
static uint8_t e22_boot_complete;

static uint8_t E22_LocalAddh(void)
{
  return (uint8_t)(e22_local_address >> 8U);
}

static uint8_t E22_LocalAddl(void)
{
  return (uint8_t)e22_local_address;
}

static void E22_ResetAttemptDiagnostics(uint8_t index)
{
  uint8_t byte_index;

  debug.e22.last_error[index] = E22_ERR_NONE;
  debug.e22.readback_ok[index] = 0U;
  debug.e22.readback_reg0[index] = 0U;
  debug.e22.readback_reg1[index] = 0U;
  debug.e22.readback_reg3[index] = 0U;
  debug.e22.readback_channel[index] = 0U;
  debug.e22.fail_step[index] = E22_STEP_NONE;
  debug.e22.rx_count[index] = 0U;
  debug.e22.uart_error[index] = 0U;
  debug.e22.readback_source[index] = 0U;
  debug.e22.c0_rx_count[index] = 0U;
  for (byte_index = 0U; byte_index < E22_COMMAND_SIZE; byte_index++)
  {
    debug.e22.reply[index][byte_index] = 0U;
  }
}

static void E22_StoreReplyDiagnostics(uint8_t index, const uint8_t *reply)
{
  uint8_t byte_index;
  uint16_t rx_count = Board_RadioLastRxCount(index);

  debug.e22.rx_count[index] = (rx_count <= E22_COMMAND_SIZE) ? (uint8_t)rx_count
                                                             : E22_COMMAND_SIZE;
  debug.e22.uart_error[index] = Board_RadioLastUartError(index);
  for (byte_index = 0U; byte_index < E22_COMMAND_SIZE; byte_index++)
  {
    debug.e22.reply[index][byte_index] = reply[byte_index];
  }
}

static void E22_StoreReadback(uint8_t index, const uint8_t *reply, uint8_t rx_ok)
{
  debug.e22.readback_ok[index] = rx_ok;
  if (rx_ok == 0U)
  {
    return;
  }

  debug.e22.readback_addh[index] = reply[3];
  debug.e22.readback_addl[index] = reply[4];
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
  if ((reply[3] != E22_LocalAddh()) || (reply[4] != E22_LocalAddl()) ||
      (reply[5] != E22_NETID) ||
      ((reply[6] & E22_UART_FIELD_MASK) != E22_UART_115200_8N1) ||
      (reply[7] != E22_REG1_240B_22DBM) || (reply[8] != E22_CHANNEL) ||
      (reply[9] != E22_REG3_FIXED_POINT) || (reply[10] != E22_CRYPT_H) ||
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
      0U,
      0U,
      E22_NETID,
      E22_REG0_115200_8N1_62K5,
      E22_REG1_240B_22DBM,
      E22_CHANNEL,
      E22_REG3_FIXED_POINT,
      E22_CRYPT_H,
      E22_CRYPT_L};
  command[3] = E22_LocalAddh();
  command[4] = E22_LocalAddl();
  uint8_t reply[E22_COMMAND_SIZE] = {0U};
  uint8_t read_command[E22_READ_COMMAND_SIZE] = {
      E22_READ_COMMAND,
      E22_REGISTER_START,
      E22_REGISTER_COUNT};
  uint8_t c0_reply_ok = 0U;

  if (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_AUX;
    debug.e22.fail_step[index] = E22_STEP_PRE_WRITE_AUX;
    return 0U;
  }

  Board_RadioDrainUart(index);
  if (Board_RadioTransmitBlock(index, command, (uint16_t)sizeof(command),
                               E22_AT_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_TX;
    debug.e22.fail_step[index] = E22_STEP_TX;
    return 0U;
  }

  if (Board_RadioReceiveBlock(index, reply, (uint16_t)sizeof(reply),
                              E22_AT_TIMEOUT_MS) != 0U)
  {
    E22_StoreReplyDiagnostics(index, reply);
    debug.e22.c0_rx_count[index] = debug.e22.rx_count[index];
    E22_StoreReadback(index, reply, E22_HeaderOk(reply));
    if (E22_PayloadAcceptable(reply) != 0U)
    {
      debug.e22.readback_source[index] = 1U;
      c0_reply_ok = 1U;
    }
  }
  else
  {
    E22_StoreReplyDiagnostics(index, reply);
    debug.e22.c0_rx_count[index] = debug.e22.rx_count[index];
    E22_StoreReadback(index, reply, 0U);
  }

  if (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_AUX;
    debug.e22.fail_step[index] = E22_STEP_POST_WRITE_AUX;
    return 0U;
  }

  if (c0_reply_ok == 0U)
  {
    uint8_t byte_index;

    /* 某些通道未返回/错过 C0 应答时，主动发 C1 做一次独立回读。 */
    for (byte_index = 0U; byte_index < E22_COMMAND_SIZE; byte_index++)
    {
      reply[byte_index] = 0U;
    }
    Board_RadioDrainUart(index);
    if (Board_RadioTransmitBlock(index, read_command, (uint16_t)sizeof(read_command),
                                 E22_AT_TIMEOUT_MS) == 0U)
    {
      debug.e22.last_error[index] = E22_ERR_TX;
      debug.e22.fail_step[index] = E22_STEP_C1_TX;
      return 0U;
    }

    if (Board_RadioReceiveBlock(index, reply, (uint16_t)sizeof(reply),
                                E22_C1_FALLBACK_TIMEOUT_MS) == 0U)
    {
      E22_StoreReplyDiagnostics(index, reply);
      E22_StoreReadback(index, reply, 0U);
      debug.e22.last_error[index] = E22_ERR_RX;
      debug.e22.fail_step[index] = E22_STEP_C1_RX;
      return 0U;
    }

    E22_StoreReplyDiagnostics(index, reply);
    E22_StoreReadback(index, reply, E22_HeaderOk(reply));
    if (E22_PayloadAcceptable(reply) == 0U)
    {
      debug.e22.last_error[index] = E22_ERR_MISMATCH;
      debug.e22.fail_step[index] = E22_STEP_C1_MISMATCH;
      return 0U;
    }
    debug.e22.readback_source[index] = 2U;

    if (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U)
    {
      debug.e22.last_error[index] = E22_ERR_AUX;
      debug.e22.fail_step[index] = E22_STEP_POST_WRITE_AUX;
      return 0U;
    }
  }

  return 1U;
}

static uint8_t E22_ConfigureOne(uint8_t index)
{
  uint8_t ok;

  E22_ResetAttemptDiagnostics(index);
  Board_RadioStopRx(index);
  /* 配置模式 UART 固定 9600。运行期改址时 MCU 可能已是 115200，必须先切回来。 */
  if (Board_RadioSetBaud(index, 9600U) == 0U)
  {
    debug.e22.last_error[index] = E22_ERR_BAUD;
    debug.e22.fail_step[index] = E22_STEP_SET_BAUD;
    debug.e22.mcu_baud[index] = 9600U;
    return 0U;
  }
  debug.e22.mcu_baud[index] = 9600U;
  Board_RadioSetConfigMode(index, 1U);
  Bsp_DelayMs(E22_MODE_SETTLE_MS);
  debug.e22.config_m0_level[index] = Board_RadioM0Level(index);
  debug.e22.config_m1_level[index] = Board_RadioM1Level(index);

  ok = E22_WriteRegisters(index);
  if ((ok != 0U) && (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U))
  {
    debug.e22.last_error[index] = E22_ERR_AUX;
    debug.e22.fail_step[index] = E22_STEP_CONFIG_AUX;
    ok = 0U;
  }

  Board_RadioSetConfigMode(index, 0U);
  Bsp_DelayMs(E22_MODE_SETTLE_MS);
  if (Board_RadioWaitAuxHigh(index, E22_AUX_TIMEOUT_MS) == 0U)
  {
    if (ok != 0U)
    {
      debug.e22.last_error[index] = E22_ERR_AUX;
      debug.e22.fail_step[index] = E22_STEP_WORK_MODE_AUX;
    }
    ok = 0U;
  }

  debug.e22.aux_level[index] = Board_RadioAuxLevel(index);

  if (ok != 0U)
  {
    /* 模块工作串口已是 115200 才改 MCU；失败则保持 CubeMX 的 9600，避免砖死。 */
    if (Board_RadioSetBaud(index, E22_WORK_BAUD) == 0U)
    {
      debug.e22.last_error[index] = E22_ERR_BAUD;
      debug.e22.fail_step[index] = E22_STEP_SET_BAUD;
      debug.e22.mcu_baud[index] = 9600U;
      return 0U;
    }
    debug.e22.mcu_baud[index] = E22_WORK_BAUD;
    debug.e22.last_error[index] = E22_ERR_NONE;
    debug.e22.fail_step[index] = E22_STEP_NONE;
    return 1U;
  }

  debug.e22.mcu_baud[index] = 9600U;
  return 0U;
}

uint8_t E22Provisioning_Run(uint16_t local_address)
{
  uint8_t index;
  uint8_t all_ok = 1U;

  e22_local_address = local_address;

  for (index = 0U; index < BOARD_RADIO_COUNT; index++)
  {
    debug.e22.ok[index] = 0U;
    debug.e22.readback_ok[index] = 0U;
    debug.e22.readback_addh[index] = 0U;
    debug.e22.readback_addl[index] = 0U;
    debug.e22.readback_reg0[index] = 0U;
    debug.e22.readback_reg1[index] = 0U;
    debug.e22.readback_reg3[index] = 0U;
    debug.e22.readback_channel[index] = 0U;
    debug.e22.attempt_count[index] = 0U;
    debug.e22.last_error[index] = E22_ERR_NONE;
    debug.e22.boot_aux_ok[index] = 0U;
    debug.e22.aux_level[index] = 0U;
    debug.e22.fail_step[index] = E22_STEP_NONE;
    debug.e22.rx_count[index] = 0U;
    debug.e22.uart_error[index] = 0U;
    debug.e22.readback_source[index] = 0U;
    debug.e22.c0_rx_count[index] = 0U;
    debug.e22.config_m0_level[index] = 0U;
    debug.e22.config_m1_level[index] = 0U;
    debug.e22.aux_pullup_level[index] = 0U;
    debug.e22.rx_idle_level[index] = 0U;
    debug.e22.rx_pulldown_level[index] = 0U;
    E22_ResetAttemptDiagnostics(index);
    debug.e22.mcu_baud[index] = 9600U;
    Board_RadioSetConfigMode(index, 0U);
  }
  debug.e22.all_ok = 0U;

  if (e22_boot_complete == 0U)
  {
    Bsp_DelayMs(E22_POWER_UP_SETTLE_MS);
    for (index = 0U; index < BOARD_RADIO_COUNT; index++)
    {
      debug.e22.boot_aux_ok[index] = Board_RadioWaitAuxHigh(index, E22_BOOT_AUX_TIMEOUT_MS);
      debug.e22.aux_level[index] = Board_RadioAuxLevel(index);
    }
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

  if (e22_boot_complete == 0U)
  {
    /* 无示波器时用内部弱上下拉做静态线路探测；随后均恢复原 GPIO/USART 配置。 */
    for (index = 0U; index < BOARD_RADIO_COUNT; index++)
    {
      debug.e22.aux_pullup_level[index] = Board_RadioProbeAuxPullup(index);
      debug.e22.rx_idle_level[index] = Board_RadioRxLevel(index);
      debug.e22.rx_pulldown_level[index] = Board_RadioProbeRxPulldown(index);
    }
  }

  e22_boot_complete = 1U;
  debug.e22.all_ok = all_ok;
  return all_ok;
}
