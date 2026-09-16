#ifndef BSP_ADC_HPP
#define BSP_ADC_HPP

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace BSP_n
{

/**
 * @brief G431 ADC 轮询封装。
 *
 * 构造时只保存句柄，不启动转换、不开 DMA。
 * 发端摇杆仍是 ADC1×3 + ADC2×1 软件触发轮询。
 */
class ADC_c
{
public:
    ADC_c(ADC_HandleTypeDef *handle);

    bool calibrate_single_ended();
    bool start();
    bool stop();
    bool poll_for_conversion(uint32_t timeout_ms);
    uint16_t value() const;

private:
    ADC_HandleTypeDef *handle_;
};

} /* namespace BSP_n */

#endif /* BSP_ADC_HPP */
