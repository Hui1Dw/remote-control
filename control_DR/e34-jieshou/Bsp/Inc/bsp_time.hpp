#ifndef BSP_TIME_HPP
#define BSP_TIME_HPP

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace BSP_n
{

/**
 * @brief 毫秒时间源。
 *
 * 当前仍基于 HAL SysTick。业务层只通过 now_ms()/delay_ms() 取时间。
 */
class Time_c
{
public:
    static uint32_t now_ms();
    static void delay_ms(uint32_t ms);
};

} /* namespace BSP_n */

#endif /* BSP_TIME_HPP */
