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
 * 运行时发送支持阻塞和中断。E22 配置阶段额外提供阻塞收发、清 RX、改波特率。
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

    /** 阻塞发送，供 E22 配置命令使用；与运行时 TxType 无关。 */
    bool transmit_block(const uint8_t *data, uint16_t size, uint32_t timeout_ms);

    /** 阻塞接收固定长度，供读 C1 回读。 */
    bool receive_block(uint8_t *data, uint16_t size, uint32_t timeout_ms);

    /** 最近一次阻塞接收实际收到的字节数（超时时也保留部分长度）。 */
    uint16_t last_rx_count() const;

    /** 最近一次阻塞接收结束时的 HAL UART ErrorCode，仅供 Board 诊断。 */
    uint32_t last_uart_error() const;

    /** 丢掉 RX 残留字节并清除错误标志，避免混进 C1。 */
    void drain_rx();

    /**
     * @brief 改 MCU UART 工作波特率。
     *
     * 只在 E22 C1 回读确认模块已是新波特率之后调用。
     * 配置模式必须保持 9600。
     */
    bool set_baud(uint32_t baud);

    /** 发端无 DMA RX，这里只 Abort，保证配置收发前 gState=READY。 */
    void stop_rx();

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
    uint16_t last_rx_count_;
    uint32_t last_uart_error_;
};

} /* namespace BSP_n */

#endif /* BSP_USART_HPP */
