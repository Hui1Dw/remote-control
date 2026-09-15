#ifndef __WIRELESS_CONTROL_H__
#define __WIRELESS_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define WIRELESS_CONTROL_FRAME_SIZE  14U

/**
 * @brief Runtime counters and most recent transmit result for E34-1 control frames.
 *
 * These variables are intended for debugger observation. A skipped frame means
 * the E34 AUX pin was low when the 50 Hz transmission slot arrived.
 */
typedef struct
{
  uint32_t tx_success_count;          /* UART 提交成功次数，正常约每 20 ms +1 */
  uint32_t tx_error_count;            /* UART 提交失败次数 */
  uint32_t tx_busy_skip_count;        /* E34 AUX 为低而跳过本发送周期的次数 */
  HAL_StatusTypeDef last_tx_status;   /* 最近一次 HAL_UART_Transmit 返回值，0=HAL_OK */
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
 * The function is non-blocking while waiting for the next transmission slot or
 * for AUX to become ready. The actual UART transfer is blocking and takes
 * about 15 ms at the current 9600 bps setting.
 */
void WirelessControl_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIRELESS_CONTROL_H__ */
