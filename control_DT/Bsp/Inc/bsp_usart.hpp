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
 * 发送支持阻塞和中断。DMA 发送、Receive-to-IDLE 仍留给后续阶段。
 * 回调使用函数指针，不用 std::function，也不在构造时启动外设。
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

    /**
     * @brief 提交一帧发送。
     *
     * BLOCK：阻塞到发完或超时。
     * IT：启动 HAL_UART_Transmit_IT 后立即返回；忙或 gState 非 READY 时返回 false，
     *     不排队、不覆盖正在发送的缓冲。
     * DMA：尚未实现，返回 false。
     *
     * timeout_ms 仅 BLOCK 使用。
     */
    bool send(const uint8_t *data, uint16_t size, uint32_t timeout_ms);

    /** IT/DMA 发送尚未完成时为 true。 */
    bool is_busy() const;

    UART_HandleTypeDef *handle();

    /* HAL 弱回调转发入口，不给 App 使用。 */
    static void handleTxCallback(UART_HandleTypeDef *huart);
    static void handleErrorCallback(UART_HandleTypeDef *huart);

private:
    static void RegisterInstance(USART_c *instance);
    static USART_c *FindInstanceByHandle(UART_HandleTypeDef *huart);

    static USART_c *instances_[3];
    static uint8_t instance_count_;

    UART_HandleTypeDef *handle_;
    TxType_t tx_type_;
    volatile bool busy_;
};

} /* namespace BSP_n */

#endif /* BSP_USART_HPP */
