#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 收端业务入口。
 *
 * 本阶段只把 main 循环里的 poll 迁到 App，不接管 UART HAL 回调。
 *
 * @return 1=成功，0=失败。
 */
uint8_t App_Init(void);

/**
 * @brief 主循环一轮：抽三路 DMA 接收、分发协议、更新 AUX 与失联保护。
 */
void App_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
