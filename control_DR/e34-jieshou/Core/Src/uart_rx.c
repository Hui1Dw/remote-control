#include "uart_rx.h"

#define UART_RX_MAX_INSTANCES 3

static UART_RX_Instance *instances[UART_RX_MAX_INSTANCES];
static uint8_t instance_count = 0;

volatile uint32_t dbg_dma_rx_len = 0;
/* Index 0/1/2 corresponds to USART1/USART2/USART3. */
volatile uint32_t uart_rx_dma_event_count[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_dma_byte_count[3] = {0U, 0U, 0U};
volatile uint16_t uart_rx_last_dma_size[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_empty_event_count[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_initial_sync_drop_count[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_dma_start_ok_count[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_dma_start_error_count[3] = {0U, 0U, 0U};
volatile int32_t  uart_rx_last_dma_start_status[3] = {0, 0, 0};
volatile uint16_t uart_rx_dma_ndtr[3] = {0U, 0U, 0U};
volatile uint16_t uart_rx_dma_live_byte_count[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_error_callback_count[3] = {0U, 0U, 0U};
volatile uint32_t uart_rx_last_error_code[3] = {0U, 0U, 0U};
extern volatile uint8_t dbg_rx1_flag;
extern volatile uint8_t dbg_rx2_flag;
extern volatile uint8_t dbg_rx3_flag;
extern volatile uint8_t dbg_rx_byte_0;
extern volatile uint8_t dbg_rx_byte_1;
extern volatile uint8_t dbg_rx_byte_2;
extern volatile uint8_t dbg_rx_byte_3;
extern volatile uint16_t dbg_rx_data_len;
extern volatile uint16_t dbg_irq_size;

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

volatile uint32_t dbg_start_dma_count = 0;
volatile uint32_t dbg_start_dma_ccr = 0;

void UART_RX_StartDMA(UART_RX_Instance *inst)
{
    if (inst == NULL || inst->huart == NULL)
    {
        return;
    }

    dbg_start_dma_count++;

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
        uart_rx_last_dma_start_status[(uint8_t)debug_index] = (int32_t)status;
        if (status == HAL_OK)
        {
            uart_rx_dma_start_ok_count[(uint8_t)debug_index]++;
            uart_rx_dma_ndtr[(uint8_t)debug_index] = UART_RX_BUF_SIZE;
            uart_rx_dma_live_byte_count[(uint8_t)debug_index] = 0U;
        }
        else
        {
            uart_rx_dma_start_error_count[(uint8_t)debug_index]++;
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
            uart_rx_dma_ndtr[(uint8_t)debug_index] = ndtr;
            uart_rx_dma_live_byte_count[(uint8_t)debug_index] =
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
        uart_rx_error_callback_count[(uint8_t)debug_index]++;
        uart_rx_last_error_code[(uint8_t)debug_index] = huart->ErrorCode;
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
        uart_rx_dma_event_count[(uint8_t)debug_index]++;
        uart_rx_last_dma_size[(uint8_t)debug_index] = Size;
    }

    /* ReceiveToIdle DMA reports only real received bytes.  Keep this guard
       for defensive handling of an unexpected zero-size HAL callback. */
    if (Size == 0U || Size > UART_RX_BUF_SIZE)
    {
        if (debug_index >= 0)
        {
            uart_rx_empty_event_count[(uint8_t)debug_index]++;
        }
        inst->rx_len = 0U;
        inst->rx_ready = 1U;
        return;
    }

    inst->first_frame_done = 1U;
    if (debug_index >= 0)
    {
        uart_rx_dma_byte_count[(uint8_t)debug_index] += Size;
    }

    dbg_dma_rx_len = Size;
    inst->rx_len = Size;
    inst->rx_ready = 1U;

    dbg_irq_size = Size;
    dbg_rx_data_len = Size;
    dbg_rx_byte_0 = inst->rx_buf[0];
    dbg_rx_byte_1 = inst->rx_buf[1];
    dbg_rx_byte_2 = inst->rx_buf[2];
    dbg_rx_byte_3 = inst->rx_buf[3];

    if (huart->Instance == USART1) dbg_rx1_flag = 1U;
    else if (huart->Instance == USART2) dbg_rx2_flag = 1U;
    else if (huart->Instance == USART3) dbg_rx3_flag = 1U;
}
