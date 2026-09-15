#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "receiver_control.h"

/**
 * @brief 单路串口已解码控制帧诊断。
 *
 * debug.link[] / debug.uart[] 下标：0=USART1，1=USART2，2=USART3。
 */
typedef struct
{
    ControlProtocol_Frame last_frame; /* 最近一帧 CRC 正确的解码结果 */
    uint8_t last_frame_valid;         /* 1=该路至少收到过完整且 CRC 正确的帧 */
    uint8_t last_frame_safe;          /* 1=摇杆范围、拨杆编码和校准位也通过安全检查 */
    uint8_t switch_state[4];          /* 拨杆1～4：1=A，2=B，0/3=无效 */
    uint8_t button_state[4];          /* 按键1～4：0=关，1=开 */
    uint8_t calibration_valid;        /* 发送端摇杆中心校准有效位，对应 BTN.bit4 */
    uint32_t valid_frame_count;       /* CRC 正确帧累计次数 */
    uint32_t crc_error_count;         /* CRC 错误累计次数 */
    uint32_t format_error_count;      /* 帧头/版本/类型/长度错误累计次数 */
    uint32_t invalid_control_count;   /* 控制内容不安全的帧累计次数 */
} DebugLink_t;

typedef struct
{
    uint32_t dma_event_count;         /* UART IDLE/DMA 接收事件次数 */
    uint32_t dma_byte_count;          /* 已提交给协议层的原始字节累计数 */
    uint16_t last_dma_size;           /* 最近一次 DMA 事件字节数，控制帧通常为 14 */
    uint32_t empty_event_count;       /* DMA 刚启动时的零字节 IDLE 事件次数 */
    uint32_t initial_sync_drop_count; /* 预留：上电同步丢弃计数，当前未写入 */
    uint32_t dma_start_ok_count;      /* Receive-to-IDLE DMA 启动成功次数 */
    uint32_t dma_start_error_count;   /* Receive-to-IDLE DMA 启动失败次数 */
    int32_t  last_dma_start_status;   /* 最近一次 DMA 启动的 HAL 返回值，0=HAL_OK */
    uint16_t dma_ndtr;                /* DMA 当前剩余传输计数，刚启动时为 256 */
    uint16_t dma_live_byte_count;     /* DMA 缓冲中已搬运、尚未等到 IDLE 提交的字节数 */
    uint32_t error_callback_count;    /* HAL 接收错误回调次数 */
    uint32_t last_error_code;         /* 最近一次 HAL 接收错误码 */
    uint32_t irq_count;               /* UART 外设中断次数 */
    uint32_t error_irq_count;         /* 奇偶/帧/噪声/溢出错误中断次数 */
    uint32_t last_isr;                /* 最近一次 UART ISR 寄存器快照 */
    uint8_t  rx_flag;                 /* 最近一次 DMA 收到有效数据后置 1 */
    uint8_t  rx_last;                 /* 该路最近交给协议层的一个字节 */
    uint32_t rx_count;                /* 该路已交给协议层的字节累计数 */
} DebugUart_t;

/**
 * @brief 接收端全部公开调试状态。
 *
 * 调试器只需 Watch 全局变量 `debug`，展开即可查看输出、三路解析结果
 * 和 UART/DMA 诊断。
 */
typedef struct
{
    ReceiverControl_Output output; /* 当前允许输出的控制值；安全状态时清零 */
    uint8_t safe;                  /* 0=正常输出，1=安全状态 */
    uint8_t selected_source;       /* 按优先级选出的候选串口；255=无。被安全检查拒绝时仍保留编号 */
    uint8_t active_source;         /* 实际已允许输出的串口编号；安全状态为 255 */
    uint8_t safe_reason;           /* 0=无，1=无在线源/超时，2=选中链路 CRC 错，3=控制内容无效 */
    uint8_t source_online[3];      /* 各路是否在超时窗口内有安全帧：0=USART1，1=USART2，2=USART3 */
    uint32_t safe_enter_count;     /* 进入安全状态的累计次数 */
    DebugLink_t link[3];           /* 三路协议诊断，下标 0/1/2 = USART1/2/3 */
    DebugUart_t uart[3];           /* 三路 UART/DMA 诊断，下标 0/1/2 = USART1/2/3 */
    uint8_t last_rx_bytes[4];      /* 最近一次 DMA 缓冲的前 4 字节，便于核对帧头 */
    uint16_t last_rx_len;          /* 最近一次 DMA 收到的字节数 */
    uint16_t last_irq_size;        /* 最近一次 IDLE 事件字节数 */
    uint32_t dma_rx_len;           /* 最近一次 DMA 提交给上层的长度 */
    uint32_t start_dma_count;      /* 启动 DMA 接收的累计次数 */
    uint8_t aux_usart1;            /* USART1 模块 AUX=PA8：1=高/空闲，0=低/忙 */
    uint8_t aux_usart2;            /* USART2 模块 AUX=PB5：1=高/空闲，0=低/忙 */
    uint8_t aux_usart3;            /* USART3 模块 AUX=PB2：1=高/空闲，0=低/忙 */
    struct
    {
        uint8_t all_ok;                  /* 1=三路 C1 回读均成功并已切 115200 */
        uint32_t mcu_baud[3];            /* 各路 MCU 工作波特率；失败保持 9600 */
        uint8_t ok[3];                   /* 各路配置成功，0=USART1 E22-400 */
        uint8_t readback_ok[3];          /* 各路收到 C1 头 */
        uint8_t readback_reg0[3];        /* 回读 REG0：高3位 UART，低3位空中速率，目标 0xE6 */
        uint8_t readback_reg1[3];        /* 回读 REG1，目标 0x00 */
        uint8_t readback_reg3[3];        /* 回读 REG3，必须 0x00 透传，禁止 0x43 */
        uint8_t readback_channel[3];     /* 回读 CHAN，目标 23 */
        uint8_t attempt_count[3];        /* 各路配置尝试次数 */
        uint8_t last_error[3];           /* 0无 1AUX超时 2TX失败 3RX失败 4回读不符 5改波特失败 */
    } e22;                             /* 上电一次性 E22 寄存器配置诊断 */
} Debug_t;

extern volatile Debug_t debug; /* 接收端公开调试状态，Watch 此变量 */

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
