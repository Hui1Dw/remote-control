#ifndef __WIRELESS_CONTROL_H__
#define __WIRELESS_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WIRELESS_CONTROL_FRAME_SIZE  9U
#define WIRELESS_E22_FIXED_HEADER_SIZE 3U
#define WIRELESS_E22_TARGET_CHANNEL    23U
#define WIRELESS_CONTROL_TX_PERIOD_DEFAULT_MS 20U
#define WIRELESS_CONTROL_TX_PERIOD_FAST_MS    10U

/**
 * @brief Runtime counters and most recent transmit result for E22-1 control frames.
 *
 * These variables are intended for debugger observation. A skipped frame means
 * the E22 AUX pin was low, or USART1 IT 发送尚未完成, when the TX
 * slot arrived.
 */
typedef struct
{
  uint32_t tx_success_count;   /* UART 提交成功次数；最快 10 ms，AUX 晚到则立刻补最新值 */
  uint32_t tx_error_count;     /* UART 提交失败次数 */
  uint32_t tx_busy_skip_count; /* 时隙到期时 AUX/UART 忙的次数；忙则保留时隙重试 */
  uint32_t tx_quiet_skip_count; /* 配对号输入期间主动暂停发送而跳过的次数 */
  uint8_t last_tx_status;      /* 最近一次提交结果，0=成功 */
  uint32_t tx_period_ms;       /* 当前发送周期。USART1 配置成功为 10，否则 20 */
  uint32_t last_submit_interval_ms; /* 两次 UART 提交成功间隔；AUX 占用决定下限 */
  uint32_t max_submit_interval_ms;  /* 上述间隔的最大值，1 ms 分辨率 */
} WirelessControlDiagnostics_t;

/**
 * @brief Initialize the control-frame sender state.
 *
 * USART1 and GPIO must already have been initialized. The selected E22-1
 * module uses USART1 and its AUX pin to determine whether it is ready.
 */
void WirelessControl_Init(void);

/**
 * @brief 设置定点发送的目标接收机地址。
 *
 * E22 REG3=0x43 时，UART 前 3 字节必须是目标 ADDH/ADDL/CHAN。
 * 出厂目标为 0x0207。
 */
void WirelessControl_SetDestination(uint16_t address);

/**
 * @brief 设置控制帧发送周期。USART1 E22 配置成功后用 10 ms，失败保持 20 ms。
 */
void WirelessControl_SetTxPeriodMs(uint32_t period_ms);

/**
 * @brief 允许或暂停 9 字节控制帧发送。
 *
 * 配对号输入期间必须暂停，使接收机按既有 300 ms 失联进入安全态。
 * enabled=0 时跳过当前周期，不排队、不覆盖。
 */
void WirelessControl_SetTxEnabled(uint8_t enabled);

/**
 * @brief Send one latest-input control frame when E22-1 is ready.
 *
 * The function is non-blocking: waiting for the next slot, AUX, or USART1 IT
 * completion all return immediately. If AUX is still low when the 10 ms slot
 * arrives, the slot is kept and retried next loop with the latest inputs.
 * HAL_UART_Transmit_IT keeps the static TX buffer until the complete callback.
 */
void WirelessControl_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIRELESS_CONTROL_H__ */
