#include "bsp_usart.hpp"

namespace BSP_n
{

USART_c *USART_c::instances_[3] = {};
uint8_t USART_c::instance_count_ = 0U;

void USART_c::RegisterInstance(USART_c *instance)
{
    if ((instance == nullptr) || (instance_count_ >= 3U))
    {
        return;
    }

    instances_[instance_count_] = instance;
    instance_count_++;
}

USART_c *USART_c::FindInstanceByHandle(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0U; i < instance_count_; i++)
    {
        if ((instances_[i] != nullptr) && (instances_[i]->handle_ == huart))
        {
            return instances_[i];
        }
    }

    return nullptr;
}

USART_c::USART_c(UART_HandleTypeDef *handle, TxType_t tx_type)
    : handle_(handle), tx_type_(tx_type), busy_(false)
{
    RegisterInstance(this);
}

UART_HandleTypeDef *USART_c::handle()
{
    return handle_;
}

bool USART_c::is_busy() const
{
    return busy_;
}

bool USART_c::send(const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    if ((handle_ == nullptr) || (data == nullptr) || (size == 0U))
    {
        return false;
    }

    switch (tx_type_)
    {
    case TxType_t::BLOCK:
        return HAL_UART_Transmit(handle_, const_cast<uint8_t *>(data), size, timeout_ms) ==
               HAL_OK;

    case TxType_t::IT:
        if (busy_ || (handle_->gState != HAL_UART_STATE_READY))
        {
            return false;
        }

        busy_ = true;
        if (HAL_UART_Transmit_IT(handle_, const_cast<uint8_t *>(data), size) != HAL_OK)
        {
            busy_ = false;
            return false;
        }
        return true;

    case TxType_t::DMA:
        /* TX DMA 本步不做：14 字节 IT 已足够把扫描从阻塞里解放出来。 */
        return false;
    }

    return false;
}

void USART_c::handleTxCallback(UART_HandleTypeDef *huart)
{
    USART_c *instance = FindInstanceByHandle(huart);
    if (instance != nullptr)
    {
        instance->busy_ = false;
    }
}

void USART_c::handleErrorCallback(UART_HandleTypeDef *huart)
{
    USART_c *instance = FindInstanceByHandle(huart);
    if (instance != nullptr)
    {
        instance->busy_ = false;
    }
}

} /* namespace BSP_n */

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BSP_n::USART_c::handleTxCallback(huart);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BSP_n::USART_c::handleErrorCallback(huart);
}
