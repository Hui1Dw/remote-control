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
    WirelessControlDiagnostics_t diagnostics;          /* E34-1 发送成功/失败/忙跳过统计 */
    uint8_t last_frame[WIRELESS_CONTROL_FRAME_SIZE];   /* 最近一次组装的 14 字节控制帧 */
    uint8_t last_frame_valid;                           /* 1=至少成功组过一帧 */
  } wireless;                                          /* E34-1 / USART1 发送诊断 */
} Debug_t;

extern volatile Debug_t debug; /* 发端公开调试状态，Watch 此变量 */

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
