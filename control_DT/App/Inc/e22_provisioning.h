#ifndef E22_PROVISIONING_H
#define E22_PROVISIONING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 上电一次性配置三路 E22。
 *
 * 配置模式串口固定 9600 8N1。只有 C1 回读确认该路 UART 已是 115200 后，
 * 才把对应 MCU USART 改到 115200。失败的通道保持 9600，不阻止业务启动。
 *
 * 写入：ADDH/ADDL=local_address、NETID=0、REG0=0xE7（115200 8N1 + 62.5k 空中）、
 * REG1=0x00（240 B / 22 dBm）、CHAN=23、REG3=0x43（定点）、CRYPT=0。
 * 230 MHz 模块最高 15.6k，可能归一化空中速率低 3 位，以回读为准。
 *
 * @param local_address 本机 16 位 ADDH/ADDL。发端出厂为 0x0107。
 * @return 1=三路都成功，0=至少一路失败。
 */
uint8_t E22Provisioning_Run(uint16_t local_address);

#ifdef __cplusplus
}
#endif

#endif /* E22_PROVISIONING_H */
