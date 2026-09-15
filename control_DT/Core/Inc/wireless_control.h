#ifndef __WIRELESS_CONTROL_H__
#define __WIRELESS_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WIRELESS_CONTROL_FRAME_SIZE  14U

/**
 * @brief Runtime counters and most recent transmit result for E34-1 control frames.
 *
 * These variables are intended for debugger observation. A skipped frame means
 * the E34 AUX pin was low, or USART1 IT 发送尚未完成, when the 50 Hz
 * transmission slot arrived.
 */
typedef struct
{
  uint32_t tx_success_count;   /* UART 提交成功次数，正常约每 20 ms +1 */
  uint32_t tx_error_count;     /* UART 提交失败次数 */
  uint32_t tx_busy_skip_count; /* AUX 为低或 USART1 仍在发送而跳过本周期的次数 */
  uint8_t last_tx_status;      /* 最近一次提交结果，0=成功 */
} WirelessControlDiagnostics_t;

/**
 * @brief Initialize the control-frame sender state.
 *
 * USART1 and GPIO must already have been initialized. The selected E34-1
 * module uses USART1 and its AUX pin to determine whether it is ready.
 */
void WirelessControl_Init(void);

/**
 * @brief Send one latest-input control frame every 20 ms when E34-1 is ready.
 *
 * The function is non-blocking: waiting for the next slot, AUX, or USART1 IT
 * completion all return immediately. The 14-byte UART transfer itself also no
 * longer blocks the CPU; HAL_UART_Transmit_IT keeps the static TX buffer until
 * the complete callback.
 */
void WirelessControl_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIRELESS_CONTROL_H__ */
