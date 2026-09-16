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

/** 立即读取 AUX 电平，不等待。 */
uint8_t Board_RadioAuxLevel(uint8_t index);

/** 读取当前 M0/M1 管脚实际输入电平，用于检查输出脚是否被短路或钳位。 */
uint8_t Board_RadioM0Level(uint8_t index);
uint8_t Board_RadioM1Level(uint8_t index);

/**
 * @brief 用 MCU 内部弱上拉探测 AUX，再恢复 CubeMX 的无上下拉输入。
 *
 * 返回 1：线路能被弱上拉拉高，通常表示悬空/开路/模块未驱动。
 * 返回 0：线路仍被外部主动拉低或对地短路。
 */
uint8_t Board_RadioProbeAuxPullup(uint8_t index);

/** 直接读取 UART RX 管脚空闲电平。正常 UART 空闲通常为高。 */
uint8_t Board_RadioRxLevel(uint8_t index);

/**
 * @brief 暂停接收并用内部弱下拉探测 RX，随后恢复 USART AF。
 *
 * 返回值只表示探测瞬间线路能否抵抗弱下拉。部分 E22 在工作模式会让 TXD
 * 呈高阻，因此该值只能与其他通道对照，不能单独判定线路通断。
 */
uint8_t Board_RadioProbeRxPulldown(uint8_t index);

void Board_RadioDrainUart(uint8_t index);

/** 阻塞发送。1=成功，0=失败。 */
uint8_t Board_RadioTransmitBlock(uint8_t index, const uint8_t *data, uint16_t size,
                                 uint32_t timeout_ms);

/** 阻塞接收固定长度。1=成功，0=失败。 */
uint8_t Board_RadioReceiveBlock(uint8_t index, uint8_t *data, uint16_t size,
                                uint32_t timeout_ms);

/** 最近一次阻塞接收实际收到的字节数。 */
uint16_t Board_RadioLastRxCount(uint8_t index);

/** 最近一次阻塞接收结束时的 HAL UART ErrorCode。 */
uint32_t Board_RadioLastUartError(uint8_t index);

/**
 * @brief 改该路 MCU UART 工作波特率。
 *
 * 只应在 C1 回读确认模块 UART 已是目标波特率后调用。1=成功，0=失败。
 */
uint8_t Board_RadioSetBaud(uint8_t index, uint32_t baud);

/** 发端无 DMA RX；Abort 后保证配置收发前 UART READY。 */
void Board_RadioStopRx(uint8_t index);

#define BOARD_BUTTON_COUNT 4U
#define BOARD_TOGGLE_COUNT 4U

/**
 * @brief ADC1/ADC2 单端硬件校准。
 *
 * 只保存句柄、不改 CubeMX 通道配置。1=成功，0=失败。
 */
uint8_t Board_JoystickCalibrate(void);

/**
 * @brief 轮询读取四轴摇杆 ADC。
 *
 * 顺序与 CubeMX rank 一致：ADC1 左X/左Y/右X，ADC2 右Y。
 * 1=成功，0=失败。失败时输出值保持不变。
 */
uint8_t Board_JoystickReadRaw(uint16_t *left_y, uint16_t *left_x,
                              uint16_t *right_y, uint16_t *right_x);

/**
 * @brief 读取低有效瞬时按键。
 *
 * index：0=SW1 PA3，1=SW2 PA5，2=SW3 PA6，3=SW4 PA7。
 * 1=按下（输入为低），0=释放或非法 index。
 */
uint8_t Board_ButtonIsPressed(uint8_t index);

/**
 * @brief 读取拨杆 A/B 触点是否为低。
 *
 * index：0～3 对应拨杆 1～4。1=该触点为低，0=高或非法 index。
 */
uint8_t Board_ToggleAIsLow(uint8_t index);
uint8_t Board_ToggleBIsLow(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
