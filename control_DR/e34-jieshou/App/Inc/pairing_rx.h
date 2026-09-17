#ifndef PAIRING_RX_H
#define PAIRING_RX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 收端配对空中事务。
 *
 * UART 回调里只解析并投递邮箱；ACK Beacon、改址、Flash 和
 * COMMITTED/OK 发送都在 Poll 里做。
 */
void PairingRx_Init(uint8_t factory_default);

/**
 * @brief 尝试把一次 UART 缓冲解析为配对帧。
 *
 * @return 1=已消费该缓冲（含校验失败以外的有效配对帧），
 *         0=不是配对帧，调用方应交给控制协议。
 */
uint8_t PairingRx_PushBuffer(uint8_t source, const uint8_t *data, uint16_t size);

/**
 * @brief 推进 ACK Beacon、改址、COMMITTED 重试和 OK Beacon。
 */
void PairingRx_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_RX_H */
