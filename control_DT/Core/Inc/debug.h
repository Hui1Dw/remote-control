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
    ButtonToggleState_t toggle;        /* 每次有效按下翻转的开/关业务状态，组包用此值 */
  } button;                            /* 四个低有效瞬时按键 SW1～SW4 */

  struct
  {
    ToggleSwitchState_t raw;           /* 本次直接解码：-1=A，1=B，2=无效 */
    ToggleSwitchState_t state;         /* 消抖后的稳定档位，组包用此值 */
  } toggle_switch;                     /* 四路两档拨杆 */

  struct
  {
    WirelessControlDiagnostics_t diagnostics;          /* E34-1 提交成功/失败/忙跳过统计；last_tx_status 0=成功 */
    uint8_t last_frame[WIRELESS_CONTROL_FRAME_SIZE];   /* 最近一次组装的 14 字节控制帧 */
    uint8_t last_frame_valid;                           /* 1=至少成功组过一帧 */
  } wireless;                                          /* E22-1 / USART1 发送诊断 */

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
