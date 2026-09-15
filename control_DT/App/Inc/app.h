#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 发端业务入口。
 *
 * CubeMX 生成的 main.c 只在 USER CODE 里调用 Board_Init / App_Init / App_Loop。
 * App 上电先配置三路 E22，再做 100 Hz 扫描；USART1 配置成功后按 10 ms 组包发送。
 *
 * @return 1=成功，0=失败。
 */
uint8_t App_Init(void);

/**
 * @brief 主循环一轮：到期则扫描输入，并尝试提交一帧无线控制。
 *
 * 发送走 USART1 中断非阻塞。本函数不会在 UART 上阻塞约 15 ms。
 */
void App_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
