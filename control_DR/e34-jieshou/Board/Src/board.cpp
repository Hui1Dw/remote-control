#include "board.h"

#include "bsp_gpio.hpp"
#include "bsp_usart.hpp"
#include "debug.h"

extern "C" {
#include "receiver_control.h"
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
}

namespace
{
constexpr uint16_t kUartRxBufSize = 256U;

BSP_n::GPIO_c usart2_aux{GPIOA, GPIO_PIN_4};

uint8_t usart1_rx_buf[kUartRxBufSize];
uint8_t usart2_rx_buf[kUartRxBufSize];
uint8_t usart3_rx_buf[kUartRxBufSize];

void on_usart1_rx(const uint8_t *data, uint16_t size);
void on_usart2_rx(const uint8_t *data, uint16_t size);
void on_usart3_rx(const uint8_t *data, uint16_t size);
void on_usart1_error(uint32_t error_code);
void on_usart2_error(uint32_t error_code);
void on_usart3_error(uint32_t error_code);

BSP_n::USART_c usart1{&huart1, usart1_rx_buf, kUartRxBufSize, on_usart1_rx,
                      on_usart1_error};
BSP_n::USART_c usart2{&huart2, usart2_rx_buf, kUartRxBufSize, on_usart2_rx,
                      on_usart2_error};
BSP_n::USART_c usart3{&huart3, usart3_rx_buf, kUartRxBufSize, on_usart3_rx,
                      on_usart3_error};

void restart_rx(uint8_t src, BSP_n::USART_c &uart)
{
    debug.start_dma_count++;

    const bool ok = uart.start_rx();
    debug.uart[src].last_dma_start_status = uart.last_start_status();
    if (ok)
    {
        debug.uart[src].dma_start_ok_count++;
        debug.uart[src].dma_ndtr = kUartRxBufSize;
        debug.uart[src].dma_live_byte_count = 0U;
    }
    else
    {
        debug.uart[src].dma_start_error_count++;
    }
}

void handle_rx(uint8_t src, BSP_n::USART_c &uart, const uint8_t *data, uint16_t size)
{
    debug.uart[src].dma_event_count++;
    debug.uart[src].last_dma_size = size;

    if ((data == nullptr) || (size == 0U) || (size > kUartRxBufSize))
    {
        debug.uart[src].empty_event_count++;
        restart_rx(src, uart);
        return;
    }

    debug.uart[src].dma_byte_count += size;
    debug.uart[src].rx_flag = 1U;
    debug.dma_rx_len = size;
    debug.last_irq_size = size;
    debug.last_rx_len = size;
    debug.last_rx_bytes[0] = data[0];
    debug.last_rx_bytes[1] = (size > 1U) ? data[1] : 0U;
    debug.last_rx_bytes[2] = (size > 2U) ? data[2] : 0U;
    debug.last_rx_bytes[3] = (size > 3U) ? data[3] : 0U;

    for (uint16_t i = 0U; i < size; i++)
    {
        debug.uart[src].rx_last = data[i];
        debug.uart[src].rx_count++;
    }

    /* 中断里只解析和更新 debug；Apply 仍在 ReceiverControl_Poll。 */
    ReceiverControl_PushBuffer(src, data, size);
    restart_rx(src, uart);
}

void handle_error(uint8_t src, BSP_n::USART_c &uart, uint32_t error_code)
{
    debug.uart[src].error_callback_count++;
    debug.uart[src].last_error_code = error_code;
    /* 帧错误/噪声/溢出会中止 Receive-to-IDLE，必须立刻重装，避免一路掉线。 */
    restart_rx(src, uart);
}

void on_usart1_rx(const uint8_t *data, uint16_t size)
{
    handle_rx(0U, usart1, data, size);
}

void on_usart2_rx(const uint8_t *data, uint16_t size)
{
    handle_rx(1U, usart2, data, size);
}

void on_usart3_rx(const uint8_t *data, uint16_t size)
{
    handle_rx(2U, usart3, data, size);
}

void on_usart1_error(uint32_t error_code)
{
    handle_error(0U, usart1, error_code);
}

void on_usart2_error(uint32_t error_code)
{
    handle_error(1U, usart2, error_code);
}

void on_usart3_error(uint32_t error_code)
{
    handle_error(2U, usart3, error_code);
}

void update_uart_live(uint8_t src, const BSP_n::USART_c &uart)
{
    const uint16_t ndtr = uart.remaining_count();
    if (ndtr <= kUartRxBufSize)
    {
        debug.uart[src].dma_ndtr = ndtr;
        debug.uart[src].dma_live_byte_count =
            static_cast<uint16_t>(kUartRxBufSize - ndtr);
    }
}
} /* namespace */

extern "C" void Board_Init(void)
{
    /* 引脚和 UART 由 CubeMX 的 MX_*_Init 完成。这里只确认静态对象已构造。 */
}

extern "C" uint8_t Board_Usart2_AuxReady(void)
{
    return usart2_aux.is_set() ? 1U : 0U;
}

extern "C" void Board_UartStartRx(void)
{
    restart_rx(0U, usart1);
    restart_rx(1U, usart2);
    restart_rx(2U, usart3);
}

extern "C" void Board_UartUpdateDebug(void)
{
    update_uart_live(0U, usart1);
    update_uart_live(1U, usart2);
    update_uart_live(2U, usart3);
    debug.aux_usart2 = Board_Usart2_AuxReady();
    /* PB3 已改作 USART2_TX。USART1 模块 AUX 本板未引出。 */
    debug.aux_usart1 = 0U;
}
