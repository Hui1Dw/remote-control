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
    : handle_(handle),
      tx_type_(tx_type),
      busy_(false),
      last_rx_count_(0U),
      last_uart_error_(0U),
      rx_buf_(nullptr),
      rx_buf_size_(0U),
      rx_callback_(nullptr),
      idle_rx_active_(false)
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

bool USART_c::transmit_block(const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    if ((handle_ == nullptr) || (data == nullptr) || (size == 0U))
    {
        return false;
    }

    busy_ = false;
    return HAL_UART_Transmit(handle_, const_cast<uint8_t *>(data), size, timeout_ms) == HAL_OK;
}

bool USART_c::receive_block(uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;

    last_rx_count_ = 0U;
    last_uart_error_ = 0U;
    if ((handle_ == nullptr) || (data == nullptr) || (size == 0U))
    {
        return false;
    }

    status = HAL_UART_Receive(handle_, data, size, timeout_ms);
    if (((status == HAL_OK) || (status == HAL_TIMEOUT)) && (handle_->RxXferCount <= size))
    {
        last_rx_count_ = (uint16_t)(size - handle_->RxXferCount);
    }
    last_uart_error_ = handle_->ErrorCode;
    return status == HAL_OK;
}

uint16_t USART_c::last_rx_count() const
{
    return last_rx_count_;
}

uint32_t USART_c::last_uart_error() const
{
    return last_uart_error_;
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

bool USART_c::set_baud(uint32_t baud)
{
    if (handle_ == nullptr)
    {
        return false;
    }

    idle_rx_active_ = false;
    busy_ = false;
    (void)HAL_UART_Abort(handle_);
    handle_->Init.BaudRate = baud;
    return HAL_UART_Init(handle_) == HAL_OK;
}

void USART_c::stop_rx()
{
    if (handle_ == nullptr)
    {
        return;
    }

    idle_rx_active_ = false;
    busy_ = false;
    (void)HAL_UART_Abort(handle_);
    drain_rx();
}

bool USART_c::arm_idle_rx()
{
    if ((handle_ == nullptr) || (rx_buf_ == nullptr) || (rx_buf_size_ == 0U) ||
        (!idle_rx_active_))
    {
        return false;
    }

    (void)HAL_UART_AbortReceive(handle_);
    return HAL_UARTEx_ReceiveToIdle_IT(handle_, rx_buf_, rx_buf_size_) == HAL_OK;
}

bool USART_c::start_idle_rx(uint8_t *buf, uint16_t size, RxCallback callback)
{
    if ((handle_ == nullptr) || (buf == nullptr) || (size == 0U) ||
        (callback == nullptr))
    {
        return false;
    }

    rx_buf_ = buf;
    rx_buf_size_ = size;
    rx_callback_ = callback;
    idle_rx_active_ = true;
    return arm_idle_rx();
}

void USART_c::stop_idle_rx()
{
    idle_rx_active_ = false;
    if (handle_ != nullptr)
    {
        (void)HAL_UART_AbortReceive(handle_);
    }
}

void USART_c::handleTxCallback(UART_HandleTypeDef *huart)
{
    USART_c *instance = FindInstanceByHandle(huart);
    if (instance != nullptr)
    {
        instance->busy_ = false;
    }
}

void USART_c::handleRxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    USART_c *instance = FindInstanceByHandle(huart);
    if (instance == nullptr)
    {
        return;
    }

    if ((instance->rx_callback_ != nullptr) && (instance->rx_buf_ != nullptr) &&
        (size > 0U))
    {
        instance->rx_callback_(instance->rx_buf_, size);
    }

    if (instance->idle_rx_active_)
    {
        (void)instance->arm_idle_rx();
    }
}

void USART_c::handleErrorCallback(UART_HandleTypeDef *huart)
{
    USART_c *instance = FindInstanceByHandle(huart);
    HAL_UART_StateTypeDef state;

    if (instance == nullptr)
    {
        return;
    }

    state = instance->handle_->gState;
    if ((state != HAL_UART_STATE_BUSY_TX) && (state != HAL_UART_STATE_BUSY_TX_RX))
    {
        instance->busy_ = false;
    }

    if (instance->idle_rx_active_)
    {
        (void)instance->arm_idle_rx();
    }
}

} /* namespace BSP_n */

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BSP_n::USART_c::handleTxCallback(huart);
}

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    BSP_n::USART_c::handleRxEventCallback(huart, Size);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BSP_n::USART_c::handleErrorCallback(huart);
}
