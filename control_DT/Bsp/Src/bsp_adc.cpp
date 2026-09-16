#include "bsp_adc.hpp"

namespace BSP_n
{

ADC_c::ADC_c(ADC_HandleTypeDef *handle) : handle_(handle)
{
}

bool ADC_c::calibrate_single_ended()
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return HAL_ADCEx_Calibration_Start(handle_, ADC_SINGLE_ENDED) == HAL_OK;
}

bool ADC_c::start()
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return HAL_ADC_Start(handle_) == HAL_OK;
}

bool ADC_c::stop()
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return HAL_ADC_Stop(handle_) == HAL_OK;
}

bool ADC_c::poll_for_conversion(uint32_t timeout_ms)
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return HAL_ADC_PollForConversion(handle_, timeout_ms) == HAL_OK;
}

uint16_t ADC_c::value() const
{
    if (handle_ == nullptr)
    {
        return 0U;
    }

    return (uint16_t)HAL_ADC_GetValue(handle_);
}

} /* namespace BSP_n */
