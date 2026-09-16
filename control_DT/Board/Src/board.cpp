#include "board.h"

#include "adc.h"
#include "bsp_adc.hpp"
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

BSP_n::ADC_c joystick_adc1{&hadc1};
BSP_n::ADC_c joystick_adc2{&hadc2};

BSP_n::GPIO_c buttons[BOARD_BUTTON_COUNT] = {
    BSP_n::GPIO_c(SW1_GPIO_Port, SW1_Pin),
    BSP_n::GPIO_c(SW2_GPIO_Port, SW2_Pin),
    BSP_n::GPIO_c(SW3_GPIO_Port, SW3_Pin),
    BSP_n::GPIO_c(SW4_GPIO_Port, SW4_Pin),
};
BSP_n::GPIO_c toggle_a[BOARD_TOGGLE_COUNT] = {
    BSP_n::GPIO_c(SWITCH_A_1_GPIO_Port, SWITCH_A_1_Pin),
    BSP_n::GPIO_c(SWITCH_A_2_GPIO_Port, SWITCH_A_2_Pin),
    BSP_n::GPIO_c(SWITCH_A_3_GPIO_Port, SWITCH_A_3_Pin),
    BSP_n::GPIO_c(SWITCH_A_4_GPIO_Port, SWITCH_A_4_Pin),
};
BSP_n::GPIO_c toggle_b[BOARD_TOGGLE_COUNT] = {
    BSP_n::GPIO_c(SWITCH_B_1_GPIO_Port, SWITCH_B_1_Pin),
    BSP_n::GPIO_c(SWITCH_B_2_GPIO_Port, SWITCH_B_2_Pin),
    BSP_n::GPIO_c(SWITCH_B_3_GPIO_Port, SWITCH_B_3_Pin),
    BSP_n::GPIO_c(SWITCH_B_4_GPIO_Port, SWITCH_B_4_Pin),
};

constexpr uint32_t kAdcPollTimeoutMs = 10U;

bool valid_index(uint8_t index)
{
    return index < BOARD_RADIO_COUNT;
}

bool valid_button(uint8_t index)
{
    return index < BOARD_BUTTON_COUNT;
}

bool valid_toggle(uint8_t index)
{
    return index < BOARD_TOGGLE_COUNT;
}

bool read_adc_rank(BSP_n::ADC_c &adc, uint16_t *out)
{
    if (!adc.poll_for_conversion(kAdcPollTimeoutMs))
    {
        return false;
    }

    *out = adc.value();
    return true;
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

extern "C" uint8_t Board_JoystickCalibrate(void)
{
    if (!joystick_adc1.calibrate_single_ended())
    {
        return 0U;
    }

    return joystick_adc2.calibrate_single_ended() ? 1U : 0U;
}

extern "C" uint8_t Board_JoystickReadRaw(uint16_t *left_y, uint16_t *left_x,
                                        uint16_t *right_y, uint16_t *right_x)
{
    uint16_t sample_left_y;
    uint16_t sample_left_x;
    uint16_t sample_right_y;
    uint16_t sample_right_x;
    bool ok;

    if (left_y == nullptr || left_x == nullptr ||
        right_y == nullptr || right_x == nullptr)
    {
        return 0U;
    }

    if (!joystick_adc1.start())
    {
        return 0U;
    }

    /* ADC1 rank 1 / PA0 = left X, rank 2 / PA1 = left Y, rank 3 / PA2 = right X. */
    ok = read_adc_rank(joystick_adc1, &sample_left_x) &&
         read_adc_rank(joystick_adc1, &sample_left_y) &&
         read_adc_rank(joystick_adc1, &sample_right_x);
    if (!joystick_adc1.stop())
    {
        ok = false;
    }
    if (!ok)
    {
        return 0U;
    }

    if (!joystick_adc2.start())
    {
        return 0U;
    }

    ok = read_adc_rank(joystick_adc2, &sample_right_y);
    if (!joystick_adc2.stop())
    {
        ok = false;
    }
    if (!ok)
    {
        return 0U;
    }

    *left_y = sample_left_y;
    *left_x = sample_left_x;
    *right_y = sample_right_y;
    *right_x = sample_right_x;
    return 1U;
}

extern "C" uint8_t Board_ButtonIsPressed(uint8_t index)
{
    if (!valid_button(index))
    {
        return 0U;
    }

    return buttons[index].is_reset() ? 1U : 0U;
}

extern "C" uint8_t Board_ToggleAIsLow(uint8_t index)
{
    if (!valid_toggle(index))
    {
        return 0U;
    }

    return toggle_a[index].is_reset() ? 1U : 0U;
}

extern "C" uint8_t Board_ToggleBIsLow(uint8_t index)
{
    if (!valid_toggle(index))
    {
        return 0U;
    }

    return toggle_b[index].is_reset() ? 1U : 0U;
}
