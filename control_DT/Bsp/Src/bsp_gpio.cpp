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

void GPIO_c::configure_input(GPIOPull_t pull)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = pin_;
    init.Mode = GPIO_MODE_INPUT;
    switch (pull)
    {
    case GPIOPull_t::UP:
        init.Pull = GPIO_PULLUP;
        break;
    case GPIOPull_t::DOWN:
        init.Pull = GPIO_PULLDOWN;
        break;
    case GPIOPull_t::NONE:
    default:
        init.Pull = GPIO_NOPULL;
        break;
    }
    HAL_GPIO_Init(port_, &init);
}

void GPIO_c::configure_alternate(uint32_t alternate)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = pin_;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = alternate;
    HAL_GPIO_Init(port_, &init);
}

} /* namespace BSP_n */
