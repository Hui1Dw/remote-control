#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 绑定本板 E34 AUX。
 *
 * 阶段 0 不接管 UART 回调，避免和现有 uart_rx.c 抢 HAL_UARTEx_RxEventCallback。
 */
void Board_Init(void);

/* USART2 对应 E34 的 AUX=PA4。高电平表示模块空闲。USART1 AUX 本板未引出。 */
uint8_t Board_Usart2_AuxReady(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
