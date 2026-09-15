#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BOARD_RADIO_COUNT 3U

/**
 * @brief 绑定本板 E22 / GPIO / UART。
 *
 * App 与现有 C 文件只通过这些函数碰硬件，不再直接使用 HAL 句柄。
 * Transmit 返回 0 表示成功提交，非 0 表示失败。
 */
void Board_Init(void);

/* E22-1 / USART1 / AUX=PA8。高电平表示模块空闲，可以提交下一帧。 */
uint8_t Board_E34_1_AuxReady(void);

/* USART1 IT 发送尚未完成。与 AUX 低都视为本周期不能提交。 */
uint8_t Board_E34_1_TxBusy(void);

/**
 * @brief 提交 E22-1 一帧。
 *
 * 当前为 USART1 中断发送：成功只表示已经启动 IT，缓冲必须保持到发送完成。
 * timeout_ms 仅阻塞模式使用，IT 模式下忽略。
 */
uint8_t Board_E34_1_Transmit(const uint8_t *data, uint16_t size, uint32_t timeout_ms);

/**
 * @brief 进入/退出 E22 配置模式。
 *
 * enable=1：M0=0、M1=1（配置模式，UART 固定 9600）。
 * enable=0：M0=0、M1=0（透传）。
 * 不要使用 M0=M1=1，那是 E34 的设置组合。
 */
void Board_RadioSetConfigMode(uint8_t index, uint8_t enable);

/** 等到 AUX 高并再稳住 2 ms。1=成功，0=超时。 */
uint8_t Board_RadioWaitAuxHigh(uint8_t index, uint32_t timeout_ms);

void Board_RadioDrainUart(uint8_t index);

/** 阻塞发送。1=成功，0=失败。 */
uint8_t Board_RadioTransmitBlock(uint8_t index, const uint8_t *data, uint16_t size,
                                 uint32_t timeout_ms);

/** 阻塞接收固定长度。1=成功，0=失败。 */
uint8_t Board_RadioReceiveBlock(uint8_t index, uint8_t *data, uint16_t size,
                                uint32_t timeout_ms);

/**
 * @brief 改该路 MCU UART 工作波特率。
 *
 * 只应在 C1 回读确认模块 UART 已是目标波特率后调用。1=成功，0=失败。
 */
uint8_t Board_RadioSetBaud(uint8_t index, uint32_t baud);

/** 发端无 DMA RX；Abort 后保证配置收发前 UART READY。 */
void Board_RadioStopRx(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
