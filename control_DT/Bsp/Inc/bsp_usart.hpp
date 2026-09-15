#ifndef BSP_USART_HPP
#define BSP_USART_HPP

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace BSP_n
{

/**
 * @brief G431 USART 薄封装。
 *
 * 阶段 0 只提供阻塞发送。IT/DMA 发送和 Receive-to-IDLE 在阶段 1
 * 再加。回调使用函数指针，不用 std::function，也不在构造时启动外设。
 */
class USART_c
{
public:
    enum class TxType_t
    {
        BLOCK = 0,
        IT,
        DMA
    };

    using TxCallback = void (*)();
    using RxCallback = void (*)(const uint8_t *data, uint16_t size);

    USART_c(UART_HandleTypeDef *handle, TxType_t tx_type);
    bool send(const uint8_t *data, uint16_t size, uint32_t timeout_ms);

    UART_HandleTypeDef *handle();

private:
    static void RegisterInstance(USART_c *instance);
    static USART_c *FindInstanceByHandle(UART_HandleTypeDef *huart);

    static USART_c *instances_[3];
    static uint8_t instance_count_;

    UART_HandleTypeDef *handle_;
    TxType_t tx_type_;
};

} /* namespace BSP_n */

#endif /* BSP_USART_HPP */
