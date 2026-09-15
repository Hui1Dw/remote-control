#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 绑定本板三路 E22 USART 与 AUX。
 *
 * App 只通过这些函数碰硬件，不再直接使用 HAL 句柄或 uart_rx。
 */
void Board_Init(void);

/* USART2 对应模块 AUX=PA4。高电平表示模块空闲。USART1 AUX 本板未引出。 */
uint8_t Board_Usart2_AuxReady(void);

/**
 * @brief 启动三路 Receive-to-IDLE DMA。
 *
 * 须在无线模块上电稳定之后调用。回调里会重新启动 DMA。
 */
void Board_UartStartRx(void);

/**
 * @brief 刷新 DMA NDTR 现场值和 AUX 调试字段。
 *
 * 协议解析已在 UART 回调里完成；主循环只看现场计数和失联保护。
 */
void Board_UartUpdateDebug(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
