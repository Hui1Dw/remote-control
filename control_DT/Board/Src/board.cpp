#include "board.h"

#include "bsp_gpio.hpp"
#include "bsp_usart.hpp"
#include "usart.h"

namespace
{
BSP_n::GPIO_c e34_1_aux{AUX_GPIO_Port, AUX_Pin};
BSP_n::USART_c e34_1_uart{&huart1, BSP_n::USART_c::TxType_t::BLOCK};
} /* namespace */

extern "C" void Board_Init(void)
{
    /* 引脚和 UART 由 CubeMX 的 MX_*_Init 完成。这里只确认静态对象已构造。 */
}

extern "C" uint8_t Board_E34_1_AuxReady(void)
{
    return e34_1_aux.is_set() ? 1U : 0U;
}

extern "C" uint8_t Board_E34_1_Transmit(const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    return e34_1_uart.send(data, size, timeout_ms) ? 0U : 1U;
}
