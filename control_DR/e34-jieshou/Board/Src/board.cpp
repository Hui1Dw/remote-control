#include "board.h"

#include "bsp_gpio.hpp"

namespace
{
BSP_n::GPIO_c usart2_aux{GPIOA, GPIO_PIN_4};
} /* namespace */

extern "C" void Board_Init(void)
{
}

extern "C" uint8_t Board_Usart2_AuxReady(void)
{
    return usart2_aux.is_set() ? 1U : 0U;
}
