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

USART_c::USART_c(UART_HandleTypeDef *handle,
                 uint8_t *rx_buf,
                 uint16_t rx_buf_size,
                 RxCallback rx_callback,
                 ErrorCallback error_callback)
    : handle_(handle),
      rx_buf_(rx_buf),
      rx_buf_size_(rx_buf_size),
      rx_callback_(rx_callback),
      error_callback_(error_callback),
      last_start_status_(static_cast<int32_t>(HAL_ERROR))
{
    RegisterInstance(this);
}

UART_HandleTypeDef *USART_c::handle()
{
    return handle_;
}

int32_t USART_c::last_start_status() const
{
    return last_start_status_;
}

uint16_t USART_c::remaining_count() const
{
    if ((handle_ == nullptr) || (handle_->hdmarx == nullptr))
    {
        return 0U;
    }

    return static_cast<uint16_t>(__HAL_DMA_GET_COUNTER(handle_->hdmarx));
}

void USART_c::drain_rx()
{
    if (handle_ == nullptr)
    {
        return;
    }

    while (__HAL_UART_GET_FLAG(handle_, UART_FLAG_RXNE) != RESET)
    {
        volatile uint32_t discard = handle_->Instance->RDR;
        (void)discard;
    }

    __HAL_UART_CLEAR_OREFLAG(handle_);
    __HAL_UART_CLEAR_FEFLAG(handle_);
    __HAL_UART_CLEAR_NEFLAG(handle_);
    __HAL_UART_CLEAR_PEFLAG(handle_);
}

void USART_c::stop_rx()
{
    if (handle_ == nullptr)
    {
        return;
    }

    (void)HAL_UART_DMAStop(handle_);
    (void)HAL_UART_Abort(handle_);
    drain_rx();
    __HAL_UART_CLEAR_FLAG(handle_,
                          UART_CLEAR_IDLEF | UART_CLEAR_OREF | UART_CLEAR_NEF |
                              UART_CLEAR_FEF | UART_CLEAR_PEF);
}

bool USART_c::transmit_block(const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    if ((handle_ == nullptr) || (data == nullptr) || (size == 0U))
    {
        return false;
    }

    return HAL_UART_Transmit(handle_, const_cast<uint8_t *>(data), size, timeout_ms) == HAL_OK;
}

bool USART_c::receive_block(uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    if ((handle_ == nullptr) || (data == nullptr) || (size == 0U))
    {
        return false;
    }

    return HAL_UART_Receive(handle_, data, size, timeout_ms) == HAL_OK;
}

bool USART_c::set_baud(uint32_t baud)
{
    if (handle_ == nullptr)
    {
        return false;
    }

    (void)HAL_UART_DMAStop(handle_);
    (void)HAL_UART_Abort(handle_);
    handle_->Init.BaudRate = baud;
    return HAL_UART_Init(handle_) == HAL_OK;
}

bool USART_c::start_rx()
{
    HAL_StatusTypeDef status;

    if ((handle_ == nullptr) || (rx_buf_ == nullptr) || (rx_buf_size_ == 0U))
    {
        last_start_status_ = static_cast<int32_t>(HAL_ERROR);
        return false;
    }

    (void)HAL_UART_DMAStop(handle_);
    /* 清掉上电残留或错误后的 IDLE/ORE，避免立刻再进一次空事件或接收被卡死。 */
    __HAL_UART_CLEAR_FLAG(handle_,
                          UART_CLEAR_IDLEF | UART_CLEAR_OREF | UART_CLEAR_NEF |
                              UART_CLEAR_FEF | UART_CLEAR_PEF);

    status = HAL_UARTEx_ReceiveToIdle_DMA(handle_, rx_buf_, rx_buf_size_);
    last_start_status_ = static_cast<int32_t>(status);
    if (status == HAL_OK)
    {
        if (handle_->hdmarx != nullptr)
        {
            __HAL_DMA_DISABLE_IT(handle_->hdmarx, DMA_IT_HT);
        }
        return true;
    }

    return false;
}

void USART_c::handleRxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    USART_c *instance = FindInstanceByHandle(huart);
    if ((instance != nullptr) && (instance->rx_callback_ != nullptr))
    {
        instance->rx_callback_(instance->rx_buf_, size);
    }
}

void USART_c::handleErrorCallback(UART_HandleTypeDef *huart)
{
    USART_c *instance = FindInstanceByHandle(huart);
    if ((instance != nullptr) && (instance->error_callback_ != nullptr))
    {
        instance->error_callback_(huart->ErrorCode);
    }
}

} /* namespace BSP_n */

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    BSP_n::USART_c::handleRxEventCallback(huart, Size);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BSP_n::USART_c::handleErrorCallback(huart);
}
