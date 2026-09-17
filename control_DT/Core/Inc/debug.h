#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "button.h"
#include "joystick.h"
#include "toggle_switch.h"
#include "wireless_control.h"

/**
 * @brief 发端全部公开调试状态。
 *
 * 调试器只需 Watch 全局变量 `debug`，展开即可查看摇杆、按键、拨杆
 * 和无线发送相关字段。
 */
typedef struct
{
  struct
  {
    JoystickRaw_t raw;                 /* 四轴 ADC 原始值，0～4095 */
    JoystickRaw_t filtered;            /* 一阶低通滤波后的 ADC 值，0～4095 */
    JoystickValue_t value;             /* 中心校准后映射值，-100～+100；未校准时为 0 */
    JoystickCalibration_t calibration; /* 中心校准进行中/有效标志及各轴中位 ADC */
  } joystick;                          /* 双摇杆四轴输入 */

  struct
  {
    ButtonState_t raw;                 /* 本次 GPIO 直接读取：0=释放，1=按下 */
    ButtonState_t physical;            /* 连续 3 次一致后的物理按键状态 */
    ButtonToggleState_t toggle;        /* 业务锁存；SW1/2/4 按下翻转，SW3 短按释放翻转 */
    uint8_t sw3_pair_enter;            /* 1=SW3 已长按 3s 请求进入配对，未翻转 toggle */
    uint32_t sw3_short_count;          /* SW3 短按释放次数 */
    uint32_t sw3_long_count;           /* SW3 长按进入配对次数 */
  } button;                            /* 四个低有效瞬时按键 SW1～SW4 */

  struct
  {
    ToggleSwitchState_t raw;           /* 本次直接解码：-1=A，1=B，2=无效 */
    ToggleSwitchState_t state;         /* 消抖后的稳定档位，组包用此值 */
  } toggle_switch;                     /* 四路两档拨杆 */

  struct
  {
    WirelessControlDiagnostics_t diagnostics;          /* E34-1 提交成功/失败/忙跳过统计；last_tx_status 0=成功 */
    uint8_t last_frame[WIRELESS_CONTROL_FRAME_SIZE];   /* 最近一次组装的 14 字节控制 payload */
    uint8_t last_frame_valid;                           /* 1=至少成功组过一帧 */
    uint8_t last_tx_size;                               /* 最近一次提交给 E22 的 UART 长度，定点应为 17 */
    uint16_t destination_address;                       /* 定点目标 ADDH/ADDL，出厂 0x0207 */
    uint8_t destination_channel;                        /* 定点目标 CHAN，当前 23 */
  } wireless;                                          /* E22-1 / USART1 发送诊断 */

  struct
  {
    uint8_t capture_active;              /* 1=SW3 已交给配对号输入，不再翻 toggle */
    uint8_t enter_pending;               /* 1=已长按进入，等待松开后再开始计第一遍数字 */
    uint8_t state;                       /* 0=IDLE，1=第一遍数字，2=第二遍数字 */
    uint8_t last_cancel_reason;          /* 0无 1超时 2第一遍空 3第二遍空 4不一致 5溢出 */
    uint8_t first_number;                /* 当前第一遍短按计数 */
    uint8_t second_number;               /* 当前第二遍短按计数 */
    uint8_t request_ready;               /* 1=两遍一致，待 App 取走 */
    uint8_t requested_pair_number;       /* 最近一次待取走的配对号 */
    uint8_t last_confirmed_pair_number;  /* 最近一次确认成功的配对号 */
    uint8_t last_prepare_valid;          /* 1=已组出 17 字节 PREPARE */
    uint8_t last_prepare_build_ok;       /* 1=地址映射和 CRC 组帧成功 */
    uint8_t tx_quiet;                    /* 1=配对号输入或空中事务期间暂停 14 字节控制帧 */
    uint8_t txn_state;                   /* 0空闲 1发PREPARE 2等COMMITTED 3等VERIFY/OK 4成功 5失败 */
    uint8_t last_rx_command;             /* 最近一次配对回包命令：2=ACK 3=COMMITTED 5=OK */
    uint8_t last_error;                  /* 0无 1非出厂 2组帧失败 3ACK超时 4COMMITTED超时 5VERIFY超时 6E22 7Flash */
    uint8_t idle_rx_ok;                  /* 1=USART1 Idle-IT 已启动，用于收 ACK */
    uint8_t retries_remaining;           /* 当前 PREPARE/VERIFY 剩余提交次数 */
    uint16_t last_transmitter_address;   /* 确认后的 TX=0x0100+N */
    uint16_t last_receiver_address;      /* 确认后的 RX=0x0200+N */
    uint32_t last_session_id;            /* 组 PREPARE 时用的 session，当前等于 now_ms */
    uint32_t start_count;                /* 进入配对号输入次数 */
    uint32_t short_press_count;          /* 配对号输入期间短按次数 */
    uint32_t confirm_count;              /* 两遍一致确认次数 */
    uint32_t cancel_count;               /* 取消次数 */
    uint32_t timeout_count;              /* 15 s 无操作超时次数 */
    uint32_t overflow_count;             /* 短按超过 63 次 */
    uint8_t last_prepare[17];            /* 最近一次组好的 17 字节配对帧 */
    uint32_t prepare_tx_count;            /* PREPARE UART 提交成功次数 */
    uint32_t ack_rx_count;                /* 收到 PREPARE_ACK 次数，含重复 */
    uint32_t committed_rx_count;          /* 收到 COMMITTED 次数 */
    uint32_t verify_tx_count;             /* VERIFY UART 提交成功次数 */
    uint32_t ok_rx_count;                 /* 收到 OK 次数 */
    uint32_t tx_skip_count;               /* AUX 低或 UART 忙而跳过配对发送的次数 */
    uint32_t success_count;               /* 完整 PREPARE→OK 成功次数 */
    uint32_t fail_count;                  /* 空中事务失败次数 */
    uint8_t flash_valid;                  /* 1=Flash 存在 CRC 有效配对记录；强制出厂也不擦除 */
    uint8_t forced_factory;               /* 1=本次启动被调试宏覆盖为出厂配置 */
    uint8_t factory_default;              /* 1=当前 RAM 运行配置是出厂默认 */
    uint8_t current_pair_number;          /* 当前运行配对号；出厂覆盖后为 0 */
    uint16_t current_transmitter_address; /* 当前本机 ADDH/ADDL，出厂 0x0107 */
    uint16_t current_receiver_address;    /* 当前目标接收机地址，出厂 0x0207 */
    uint32_t current_generation;          /* Flash generation；出厂覆盖后为 0 */
    uint8_t reg3;                         /* 本次写入 E22 的 REG3，定点 0x43 */
  } pairing;                             /* SW3 配对号输入 + 空中事务 + 当前定点身份 */

  struct
  {
    uint8_t all_ok;                  /* 1=三路 C1 回读均成功并已切 115200 */
    uint32_t mcu_baud[3];            /* 各路 MCU 工作波特率；失败保持 9600 */
    uint8_t ok[3];                   /* 各路配置成功，0=USART1 E22-400 */
    uint8_t readback_ok[3];          /* 各路收到 C1 头 */
    uint8_t readback_addh[3];         /* 回读 ADDH，发端出厂 0x01 */
    uint8_t readback_addl[3];         /* 回读 ADDL，发端出厂 0x07 */
    uint8_t readback_reg0[3];        /* 回读 REG0：400/900 目标 0xE7；230 只保证高5位 0xE0 */
    uint8_t readback_reg1[3];        /* 回读 REG1，目标 0x00 */
    uint8_t readback_reg3[3];        /* 回读 REG3，定点必须 0x43 */
    uint8_t readback_channel[3];     /* 回读 CHAN，目标 23 */
    uint8_t attempt_count[3];        /* 各路配置尝试次数 */
    uint8_t last_error[3];           /* 0无 1AUX超时 2TX失败 3RX失败 4回读不符 5改波特失败 */
    uint8_t boot_aux_ok[3];          /* 上电等待阶段是否看到 AUX 高 */
    uint8_t aux_level[3];            /* 最近一次尝试结束时的 AUX 电平 */
    uint8_t fail_step[3];            /* 0无 1写前AUX 2TX 3RX 4内容 5写后AUX 6配置态AUX 7退配置AUX 8改波特 9~11主动C1 */
    uint8_t rx_count[3];             /* 最近一次 C1 接收实际收到的字节数，目标 12 */
    uint8_t reply[3][12];            /* 最近一次 C1 原始回包；未收到部分保持 0 */
    uint32_t uart_error[3];          /* 最近一次接收结束时的 HAL UART ErrorCode */
    uint8_t readback_source[3];      /* 0无有效回读，1=C0直接应答，2=主动发送C1所得回读 */
    uint8_t c0_rx_count[3];          /* C0 后第一次接收的字节数；随后主动 C1 可能覆盖 rx_count/reply */
    uint8_t config_m0_level[3];      /* 配置模式稳定后 M0 实际读值，目标 0 */
    uint8_t config_m1_level[3];      /* 配置模式稳定后 M1 实际读值，目标 1 */
    uint8_t aux_pullup_level[3];     /* AUX 加内部弱上拉后的读值：1偏向悬空，0偏向外部强拉低 */
    uint8_t rx_idle_level[3];        /* USART RX 正常复用状态下的空闲电平，通常应为 1 */
    uint8_t rx_pulldown_level[3];    /* RX 加内部弱下拉后的读值；只表示当时驱动强弱，不能单独判定通断 */
  } e22;                             /* 上电一次性 E22 寄存器配置诊断 */
} Debug_t;

extern volatile Debug_t debug; /* 发端公开调试状态，Watch 此变量 */

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
