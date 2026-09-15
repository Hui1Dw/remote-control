/* Unused debug bypass: not compiled. USART_c + Board now own UART RX. */
#include "rx_dispatcher.h"
#include <stddef.h>

#define DISPATCHER_BUF_SIZE 512U

typedef struct {
    uint8_t src;
    uint8_t data;
} DispatchEntry;

static DispatchEntry ring_buf[DISPATCHER_BUF_SIZE];
static volatile uint16_t head = 0U;
static volatile uint16_t tail = 0U;

static RX_Dispatcher_Handler user_handler = NULL;

void RX_Dispatcher_Init(void)
{
    head = 0;
    tail = 0;
}

void RX_Dispatcher_Push(uint8_t src, uint8_t byte)
{
    uint16_t next = (uint16_t)((head + 1U) % DISPATCHER_BUF_SIZE);
    if (next != tail)
    {
        ring_buf[head].src  = src;
        ring_buf[head].data = byte;
        head = next;
    }
}

void RX_Dispatcher_Poll(void)
{
    while (head != tail)
    {
        DispatchEntry entry = ring_buf[tail];
        tail = (uint16_t)((tail + 1U) % DISPATCHER_BUF_SIZE);

        if (user_handler != NULL)
        {
            user_handler(entry.src, entry.data);
        }
    }
}

void RX_Dispatcher_RegisterHandler(RX_Dispatcher_Handler handler)
{
    user_handler = handler;
}