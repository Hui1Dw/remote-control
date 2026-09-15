#ifndef UART_RX_H
#define UART_RX_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define UART_RX_BUF_SIZE 256

typedef void (*UART_RX_FrameHandler)(uint8_t *data, uint16_t len);

typedef struct {
    UART_HandleTypeDef  *huart;
    uint8_t              rx_buf[UART_RX_BUF_SIZE];
    volatile uint16_t    rx_len;
    volatile uint8_t     rx_ready;
    volatile uint32_t    rx_frame_count;
    volatile uint8_t     first_frame_done;
    UART_RX_FrameHandler frame_handler;
} UART_RX_Instance;

void     UART_RX_Init(UART_RX_Instance *inst);
void     UART_RX_Poll(UART_RX_Instance *inst);
void     UART_RX_RegisterHandler(UART_RX_Instance *inst, UART_RX_FrameHandler handler);
uint32_t UART_RX_GetFrameCount(UART_RX_Instance *inst);
void     UART_RX_StartDMA(UART_RX_Instance *inst);

/* Per-UART DMA/IDLE diagnostics: indexes 0/1/2 are USART1/USART2/USART3. */
extern volatile uint32_t uart_rx_dma_event_count[3];
extern volatile uint32_t uart_rx_dma_byte_count[3];
extern volatile uint16_t uart_rx_last_dma_size[3];
extern volatile uint32_t uart_rx_empty_event_count[3];
/* DMA startup/result diagnostics.  A non-zero error count means that DMA
   reception never armed or was re-armed unsuccessfully. */
extern volatile uint32_t uart_rx_dma_start_ok_count[3];
extern volatile uint32_t uart_rx_dma_start_error_count[3];
extern volatile int32_t  uart_rx_last_dma_start_status[3];
/* Live DMA progress: updated in UART_RX_Poll(), useful when sender has no
   inter-frame IDLE time and thus no RxEvent callback has occurred yet. */
extern volatile uint16_t uart_rx_dma_ndtr[3];
extern volatile uint16_t uart_rx_dma_live_byte_count[3];
/* HAL error diagnostics/recovery. */
extern volatile uint32_t uart_rx_error_callback_count[3];
extern volatile uint32_t uart_rx_last_error_code[3];
extern volatile uint32_t uart_rx_irq_count[3];
extern volatile uint32_t uart_rx_error_irq_count[3];
extern volatile uint32_t uart_rx_last_isr[3];

#endif