#include "bsp_gpio.hpp"

namespace BSP_n
{

GPIO_c::GPIO_c(GPIO_TypeDef *port, uint16_t pin) : port_(port), pin_(pin)
{
}

bool GPIO_c::is_set() const
{
    return HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET;
}

bool GPIO_c::is_reset() const
{
    return HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_RESET;
}

void GPIO_c::set()
{
    HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_SET);
}

void GPIO_c::reset()
{
    HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_RESET);
}

void GPIO_c::write(bool high)
{
    HAL_GPIO_WritePin(port_, pin_, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

} /* namespace BSP_n */
