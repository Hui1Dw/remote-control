#include "uart_rx.h"
#include "debug.h"

#define UART_RX_MAX_INSTANCES 3

static UART_RX_Instance *instances[UART_RX_MAX_INSTANCES];
static uint8_t instance_count = 0;

static int8_t UART_RX_GetDebugIndex(USART_TypeDef *instance);

static UART_RX_Instance *find_instance(USART_TypeDef *usart)
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        if (instances[i]->huart->Instance == usart)
            return instances[i];
    }
    return NULL;
}

void UART_RX_Init(UART_RX_Instance *inst)
{
    if (instance_count >= UART_RX_MAX_INSTANCES) return;
    instances[instance_count++] = inst;
    
    inst->rx_len = 0;
    inst->rx_ready = 0;
    inst->rx_frame_count = 0;
    inst->first_frame_done = 0;
    
    for (int i = 0; i < UART_RX_BUF_SIZE; i++)
    {
        inst->rx_buf[i] = 0;
    }
    
    UART_RX_StartDMA(inst);
}

void UART_RX_StartDMA(UART_RX_Instance *inst)
{
    if (inst == NULL || inst->huart == NULL)
    {
        return;
    }

    debug.start_dma_count++;

    /*
     * Use the HAL Receive-to-IDLE mode rather than calculating DMA NDTR in a
     * custom UART IRQ handler.  HAL ignores a pending IDLE flag with zero
     * bytes, so it cannot manufacture a 256-byte startup packet.
     */
    int8_t debug_index = UART_RX_GetDebugIndex(inst->huart->Instance);
    HAL_StatusTypeDef status;

    (void)HAL_UART_DMAStop(inst->huart);
    /* Clear a stale startup/error condition before enabling IDLE interrupt.
       Without this, an old IDLE/ORE flag can generate one misleading UART IRQ
       immediately after reset and can leave reception aborted after an error. */
    __HAL_UART_CLEAR_FLAG(inst->huart, UART_CLEAR_IDLEF | UART_CLEAR_OREF |
                          UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);

    status = HAL_UARTEx_ReceiveToIdle_DMA(inst->huart, inst->rx_buf,
                                           UART_RX_BUF_SIZE);
    if (debug_index >= 0)
    {
        debug.uart[(uint8_t)debug_index].last_dma_start_status = (int32_t)status;
        if (status == HAL_OK)
        {
            debug.uart[(uint8_t)debug_index].dma_start_ok_count++;
            debug.uart[(uint8_t)debug_index].dma_ndtr = UART_RX_BUF_SIZE;
            debug.uart[(uint8_t)debug_index].dma_live_byte_count = 0U;
        }
        else
        {
            debug.uart[(uint8_t)debug_index].dma_start_error_count++;
        }
    }

    if (status == HAL_OK)
    {
        /* A half-transfer callback is not a frame boundary. */
        if (inst->huart->hdmarx != NULL)
        {
            __HAL_DMA_DISABLE_IT(inst->huart->hdmarx, DMA_IT_HT);
        }
    }
}

void UART_RX_Poll(UART_RX_Instance *inst)
{
    int8_t debug_index;

    if (inst == NULL || inst->huart == NULL)
    {
        return;
    }

    /* This is deliberately sampled before handling rx_ready.  It proves that
       bytes reached the UART/DMA even if the transmitter emits a continuous
       stream without an IDLE gap. */
    debug_index = UART_RX_GetDebugIndex(inst->huart->Instance);
    if ((debug_index >= 0) && (inst->huart->hdmarx != NULL))
    {
        uint16_t ndtr = (uint16_t)__HAL_DMA_GET_COUNTER(inst->huart->hdmarx);
        if (ndtr <= UART_RX_BUF_SIZE)
        {
            debug.uart[(uint8_t)debug_index].dma_ndtr = ndtr;
            debug.uart[(uint8_t)debug_index].dma_live_byte_count =
                (uint16_t)(UART_RX_BUF_SIZE - ndtr);
        }
    }

    if (inst->rx_ready)
    {
        inst->rx_ready = 0;
        
        uint16_t len = inst->rx_len;
        inst->rx_frame_count++;
        
        if (len > 0 && inst->frame_handler != NULL)
        {
            inst->frame_handler(inst->rx_buf, len);
        }
        
        UART_RX_StartDMA(inst);
    }
}

void UART_RX_RegisterHandler(UART_RX_Instance *inst, UART_RX_FrameHandler handler)
{
    inst->frame_handler = handler;
}

uint32_t UART_RX_GetFrameCount(UART_RX_Instance *inst)
{
    return inst->rx_frame_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    UART_RX_Instance *inst = find_instance(huart->Instance);
    int8_t debug_index = UART_RX_GetDebugIndex(huart->Instance);

    if (debug_index >= 0)
    {
        debug.uart[(uint8_t)debug_index].error_callback_count++;
        debug.uart[(uint8_t)debug_index].last_error_code = huart->ErrorCode;
    }

    /* A framing/noise/overrun error aborts Receive-to-IDLE DMA in HAL.  Re-arm
       that UART so one bad byte cannot disable the whole redundant channel. */
    if (inst != NULL)
    {
        UART_RX_StartDMA(inst);
    }
}

static int8_t UART_RX_GetDebugIndex(USART_TypeDef *instance)
{
    if (instance == USART1) return 0;
    if (instance == USART2) return 1;
    if (instance == USART3) return 2;
    return -1;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    UART_RX_Instance *inst = find_instance(huart->Instance);
    int8_t debug_index = UART_RX_GetDebugIndex(huart->Instance);

    if (inst == NULL)
    {
        return;
    }

    if (debug_index >= 0)
    {
        debug.uart[(uint8_t)debug_index].dma_event_count++;
        debug.uart[(uint8_t)debug_index].last_dma_size = Size;
    }

    /* ReceiveToIdle DMA reports only real received bytes.  Keep this guard
       for defensive handling of an unexpected zero-size HAL callback. */
    if (Size == 0U || Size > UART_RX_BUF_SIZE)
    {
        if (debug_index >= 0)
        {
            debug.uart[(uint8_t)debug_index].empty_event_count++;
        }
        inst->rx_len = 0U;
        inst->rx_ready = 1U;
        return;
    }

    inst->first_frame_done = 1U;
    if (debug_index >= 0)
    {
        debug.uart[(uint8_t)debug_index].dma_byte_count += Size;
    }

    debug.dma_rx_len = Size;
    inst->rx_len = Size;
    inst->rx_ready = 1U;

    debug.last_irq_size = Size;
    debug.last_rx_len = Size;
    debug.last_rx_bytes[0] = inst->rx_buf[0];
    debug.last_rx_bytes[1] = inst->rx_buf[1];
    debug.last_rx_bytes[2] = inst->rx_buf[2];
    debug.last_rx_bytes[3] = inst->rx_buf[3];

    if (debug_index >= 0)
    {
        debug.uart[(uint8_t)debug_index].rx_flag = 1U;
    }
}
