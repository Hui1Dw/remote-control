#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 绑定本板 E34 / GPIO / UART。
 *
 * App 与现有 C 文件只通过这些函数碰硬件，不再直接使用 HAL 句柄。
 * Transmit 返回 0 表示成功提交，非 0 表示失败。
 */
void Board_Init(void);

/* E34-1 / USART1 / AUX=PA8。高电平表示模块空闲，可以提交下一帧。 */
uint8_t Board_E34_1_AuxReady(void);

/* USART1 IT 发送尚未完成。与 AUX 低都视为本周期不能提交。 */
uint8_t Board_E34_1_TxBusy(void);

/**
 * @brief 提交 E34-1 一帧。
 *
 * 当前为 USART1 中断发送：成功只表示已经启动 IT，缓冲必须保持到发送完成。
 * timeout_ms 仅阻塞模式使用，IT 模式下忽略。
 */
uint8_t Board_E34_1_Transmit(const uint8_t *data, uint16_t size, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
