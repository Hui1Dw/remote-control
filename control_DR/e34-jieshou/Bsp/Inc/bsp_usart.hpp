#ifndef BSP_USART_HPP
#define BSP_USART_HPP

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace BSP_n
{

/**
 * @brief G431 USART 接收封装。
 *
 * 当前只做 Receive-to-IDLE DMA。不在构造时启动 DMA，也不使用
 * std::function / 堆。发送留给后续 E22 配置阶段。
 */
class USART_c
{
public:
    using RxCallback = void (*)(const uint8_t *data, uint16_t size);
    using ErrorCallback = void (*)(uint32_t error_code);

    USART_c(UART_HandleTypeDef *handle,
            uint8_t *rx_buf,
            uint16_t rx_buf_size,
            RxCallback rx_callback,
            ErrorCallback error_callback);

    /**
     * @brief 启动或重新启动 Receive-to-IDLE DMA。
     *
     * 先停 DMA、清 IDLE/错误标志，再启动；成功后关闭 HT 中断，
     * 避免半满事件被当成一帧。
     */
    bool start_rx();

    /** DMA 剩余传输计数 NDTR。 */
    uint16_t remaining_count() const;

    /** 最近一次 start_rx() 的 HAL 返回值，0=HAL_OK。 */
    int32_t last_start_status() const;

    UART_HandleTypeDef *handle();

    /* HAL 弱回调转发入口，不给 App 使用。 */
    static void handleRxEventCallback(UART_HandleTypeDef *huart, uint16_t size);
    static void handleErrorCallback(UART_HandleTypeDef *huart);

private:
    static void RegisterInstance(USART_c *instance);
    static USART_c *FindInstanceByHandle(UART_HandleTypeDef *huart);

    static USART_c *instances_[3];
    static uint8_t instance_count_;

    UART_HandleTypeDef *handle_;
    uint8_t *rx_buf_;
    uint16_t rx_buf_size_;
    RxCallback rx_callback_;
    ErrorCallback error_callback_;
    int32_t last_start_status_;
};

} /* namespace BSP_n */

#endif /* BSP_USART_HPP */
