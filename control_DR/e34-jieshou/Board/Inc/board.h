#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BOARD_RADIO_COUNT 3U
#define BOARD_PAIRING_SLOT_COUNT 2U
#define BOARD_PAIRING_SLOT_SIZE  0x800U

/**
 * @brief 绑定本板三路 E22 USART 与 AUX。
 *
 * App 只通过这些函数碰硬件，不再直接使用 HAL 句柄或 uart_rx。
 */
void Board_Init(void);

/* USART2 对应模块 AUX=PB5。高电平表示模块空闲。 */
uint8_t Board_Usart2_AuxReady(void);

/**
 * @brief 启动三路 Receive-to-IDLE DMA。
 *
 * 须在无线模块上电稳定、E22 配置完成之后调用。回调里会重新启动 DMA。
 */
void Board_UartStartRx(void);

/**
 * @brief 刷新 DMA NDTR 现场值和 AUX 调试字段。
 *
 * 协议解析已在 UART 回调里完成；主循环只看现场计数和失联保护。
 */
void Board_UartUpdateDebug(void);

/**
 * @brief 进入/退出 E22 配置模式。
 *
 * enable=1：M0=0、M1=1（配置模式，UART 固定 9600）。
 * enable=0：M0=0、M1=0（透传）。
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

/** 配置前停 DMA RX，避免和阻塞收发抢数据。 */
void Board_RadioStopRx(uint8_t index);

/** 立即读取 AUX 电平，不等待。1=高/空闲。 */
uint8_t Board_RadioAuxLevel(uint8_t index);

/** 该路 IT 发送尚未完成。 */
uint8_t Board_RadioTxBusy(uint8_t index);

/**
 * @brief 提交该路一帧 IT 发送。
 *
 * 返回 0 表示已启动，非 0 表示忙或失败。缓冲必须保持到发送完成。
 */
uint8_t Board_RadioTransmit(uint8_t index, const uint8_t *data, uint16_t size);

uint32_t Board_NowMs(void);

/** Map pairing-journal slot 0/1 to the reserved last two 2-KiB flash pages. */
const uint8_t *Board_PairingSlotBytes(uint8_t slot);

/** Erase one reserved pairing-journal page. 1=success. */
uint8_t Board_PairingEraseSlot(uint8_t slot);

/** Program one reserved pairing-journal page from a doubleword-aligned buffer. */
uint8_t Board_PairingProgramSlot(uint8_t slot, const uint8_t *data, uint32_t length);

/** Non-blocking USB CDC write. 1=accepted, 0=not configured / TX busy / dropped. */
uint8_t Board_UsbWrite(const uint8_t *data, uint16_t len);

/** Non-blocking USB CDC read into caller buffer. Returns bytes copied. */
uint16_t Board_UsbRead(uint8_t *data, uint16_t max_len);

uint8_t Board_UsbConfigured(void);
uint8_t Board_UsbHostOpen(void);

/** Refresh debug.usb.configured / debug.usb.dtr from the USB stack. */
void Board_UsbUpdateDebug(void);

void Board_SystemReset(void);


#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
