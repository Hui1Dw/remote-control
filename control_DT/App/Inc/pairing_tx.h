#ifndef PAIRING_TX_H
#define PAIRING_TX_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 发端配对号输入和空中事务。
 *
 * SW3 长按 3 s 进入后，短按计数、长按 2 s 确认、两遍一致才开始
 * PREPARE → PREPARE_ACK → COMMITTED → VERIFY → OK。
 * 空中事务只走 USART1；AUX 低或 UART 忙则跳过，不排队。
 */
void PairingTx_Init(void);

/**
 * @brief 在 100 Hz 扫描里消费 SW3 边沿并刷新配对 Watch。
 *
 * 进入配对号输入或空中事务后暂停 14 字节控制帧。
 */
void PairingTx_Update(void);

/**
 * @brief 在主循环里推进配对发送、ACK 解析、改址和 Flash。
 *
 * 中断里只收包；E22 配置和 Flash 只在这里做。
 */
void PairingTx_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_TX_H */
