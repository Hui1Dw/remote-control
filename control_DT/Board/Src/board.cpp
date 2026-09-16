#include "board.h"

#include "bsp_gpio.hpp"
#include "bsp_time.hpp"
#include "bsp_usart.hpp"
#include "usart.h"

namespace
{
constexpr uint32_t kAuxSettleMs = 2U;

BSP_n::USART_c usart1{&huart1, BSP_n::USART_c::TxType_t::IT};
BSP_n::USART_c usart2{&huart2, BSP_n::USART_c::TxType_t::BLOCK};
BSP_n::USART_c usart3{&huart3, BSP_n::USART_c::TxType_t::BLOCK};

BSP_n::GPIO_c radio_m0[BOARD_RADIO_COUNT] = {
    BSP_n::GPIO_c(M0_GPIO_Port, M0_Pin),
    BSP_n::GPIO_c(M0_2_GPIO_Port, M0_2_Pin),
    BSP_n::GPIO_c(M0_3_GPIO_Port, M0_3_Pin),
};
BSP_n::GPIO_c radio_m1[BOARD_RADIO_COUNT] = {
    BSP_n::GPIO_c(M1_GPIO_Port, M1_Pin),
    BSP_n::GPIO_c(M1_2_GPIO_Port, M1_2_Pin),
    BSP_n::GPIO_c(M1_3_GPIO_Port, M1_3_Pin),
};
BSP_n::GPIO_c radio_aux[BOARD_RADIO_COUNT] = {
    BSP_n::GPIO_c(AUX_GPIO_Port, AUX_Pin),
    BSP_n::GPIO_c(AUX_2_GPIO_Port, AUX_2_Pin),
    BSP_n::GPIO_c(AUX_3_GPIO_Port, AUX_3_Pin),
};
BSP_n::GPIO_c radio_rx[BOARD_RADIO_COUNT] = {
    BSP_n::GPIO_c(GPIOA, GPIO_PIN_10),
    BSP_n::GPIO_c(GPIOB, GPIO_PIN_3),
    BSP_n::GPIO_c(GPIOB, GPIO_PIN_11),
};
constexpr uint32_t radio_rx_af[BOARD_RADIO_COUNT] = {
    GPIO_AF7_USART1,
    GPIO_AF7_USART2,
    GPIO_AF7_USART3,
};
BSP_n::USART_c *radio_uart[BOARD_RADIO_COUNT] = {&usart1, &usart2, &usart3};

bool valid_index(uint8_t index)
{
    return index < BOARD_RADIO_COUNT;
}
} /* namespace */

extern "C" void Board_Init(void)
{
    /* 引脚和 UART 由 CubeMX 的 MX_*_Init 完成。这里只确认静态对象已构造。 */
}

extern "C" uint8_t Board_E34_1_AuxReady(void)
{
    return radio_aux[0].is_set() ? 1U : 0U;
}

extern "C" uint8_t Board_E34_1_TxBusy(void)
{
    return radio_uart[0]->is_busy() ? 1U : 0U;
}

extern "C" uint8_t Board_E34_1_Transmit(const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    return radio_uart[0]->send(data, size, timeout_ms) ? 0U : 1U;
}

extern "C" void Board_RadioSetConfigMode(uint8_t index, uint8_t enable)
{
    if (!valid_index(index))
    {
        return;
    }

    /* E22 配置模式是 M0=0、M1=1；透传是两者都低。 */
    radio_m0[index].reset();
    radio_m1[index].write(enable != 0U);
}

extern "C" uint8_t Board_RadioWaitAuxHigh(uint8_t index, uint32_t timeout_ms)
{
    uint32_t start;

    if (!valid_index(index))
    {
        return 0U;
    }

    start = BSP_n::Time_c::now_ms();
    while ((BSP_n::Time_c::now_ms() - start) < timeout_ms)
    {
        if (radio_aux[index].is_set())
        {
            BSP_n::Time_c::delay_ms(kAuxSettleMs);
            return 1U;
        }
    }

    return 0U;
}

extern "C" uint8_t Board_RadioAuxLevel(uint8_t index)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_aux[index].is_set() ? 1U : 0U;
}

extern "C" uint8_t Board_RadioM0Level(uint8_t index)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_m0[index].is_set() ? 1U : 0U;
}

extern "C" uint8_t Board_RadioM1Level(uint8_t index)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_m1[index].is_set() ? 1U : 0U;
}

extern "C" uint8_t Board_RadioProbeAuxPullup(uint8_t index)
{
    uint8_t level;

    if (!valid_index(index))
    {
        return 0U;
    }

    radio_aux[index].configure_input(BSP_n::GPIOPull_t::UP);
    BSP_n::Time_c::delay_ms(kAuxSettleMs);
    level = radio_aux[index].is_set() ? 1U : 0U;
    radio_aux[index].configure_input(BSP_n::GPIOPull_t::NONE);
    BSP_n::Time_c::delay_ms(kAuxSettleMs);
    return level;
}

extern "C" uint8_t Board_RadioRxLevel(uint8_t index)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_rx[index].is_set() ? 1U : 0U;
}

extern "C" uint8_t Board_RadioProbeRxPulldown(uint8_t index)
{
    uint8_t level;

    if (!valid_index(index))
    {
        return 0U;
    }

    radio_uart[index]->stop_rx();
    radio_rx[index].configure_input(BSP_n::GPIOPull_t::DOWN);
    BSP_n::Time_c::delay_ms(kAuxSettleMs);
    level = radio_rx[index].is_set() ? 1U : 0U;
    radio_rx[index].configure_alternate(radio_rx_af[index]);
    BSP_n::Time_c::delay_ms(kAuxSettleMs);
    return level;
}

extern "C" void Board_RadioDrainUart(uint8_t index)
{
    if (!valid_index(index))
    {
        return;
    }

    radio_uart[index]->drain_rx();
}

extern "C" uint8_t Board_RadioTransmitBlock(uint8_t index, const uint8_t *data, uint16_t size,
                                           uint32_t timeout_ms)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_uart[index]->transmit_block(data, size, timeout_ms) ? 1U : 0U;
}

extern "C" uint8_t Board_RadioReceiveBlock(uint8_t index, uint8_t *data, uint16_t size,
                                          uint32_t timeout_ms)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_uart[index]->receive_block(data, size, timeout_ms) ? 1U : 0U;
}

extern "C" uint16_t Board_RadioLastRxCount(uint8_t index)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_uart[index]->last_rx_count();
}

extern "C" uint32_t Board_RadioLastUartError(uint8_t index)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_uart[index]->last_uart_error();
}

extern "C" uint8_t Board_RadioSetBaud(uint8_t index, uint32_t baud)
{
    if (!valid_index(index))
    {
        return 0U;
    }

    return radio_uart[index]->set_baud(baud) ? 1U : 0U;
}

extern "C" void Board_RadioStopRx(uint8_t index)
{
    if (!valid_index(index))
    {
        return;
    }

    radio_uart[index]->stop_rx();
}
