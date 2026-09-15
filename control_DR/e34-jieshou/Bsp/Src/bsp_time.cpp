#include "bsp_time.hpp"
#include "bsp.h"

namespace BSP_n
{

uint32_t Time_c::now_ms()
{
    return HAL_GetTick();
}

void Time_c::delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

} /* namespace BSP_n */

extern "C" uint32_t Bsp_NowMs(void)
{
    return BSP_n::Time_c::now_ms();
}

extern "C" void Bsp_DelayMs(uint32_t ms)
{
    BSP_n::Time_c::delay_ms(ms);
}
