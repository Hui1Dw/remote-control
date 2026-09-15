#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 收端业务入口。
 *
 * CubeMX 生成的 main.c 只在 USER CODE 里调用 Board_Init / App_Init / App_Loop。
 * App 上电先配置三路 E22，再启动 DMA 收包；不再绑定 huart*。
 *
 * @return 1=成功，0=失败。
 */
uint8_t App_Init(void);

/**
 * @brief 主循环一轮：刷新 DMA 现场值 / AUX，并做优先级与失联保护。
 */
void App_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
