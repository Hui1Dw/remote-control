#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint16_t left_y;   /* 左摇杆 Y：PA1 / ADC1_IN2，0～4095 */
  uint16_t left_x;   /* 左摇杆 X：PA0 / ADC1_IN1，0～4095 */
  uint16_t right_y;  /* 右摇杆 Y：PA4 / ADC2_IN17，0～4095 */
  uint16_t right_x;  /* 右摇杆 X：PA2 / ADC1_IN3，0～4095 */
} JoystickRaw_t;

typedef struct
{
  int16_t left_y;   /* 左摇杆 Y，中心校准后 -100～+100 */
  int16_t left_x;   /* 左摇杆 X，中心校准后 -100～+100 */
  int16_t right_y;  /* 右摇杆 Y，中心校准后 -100～+100 */
  int16_t right_x;  /* 右摇杆 X，中心校准后 -100～+100 */
} JoystickValue_t;

typedef struct
{
  uint16_t left_y_center;   /* 左摇杆 Y 中位 ADC，有效约 512～3583 */
  uint16_t left_x_center;   /* 左摇杆 X 中位 ADC，有效约 512～3583 */
  uint16_t right_y_center;  /* 右摇杆 Y 中位 ADC，有效约 512～3583 */
  uint16_t right_x_center;  /* 右摇杆 X 中位 ADC，有效约 512～3583 */
  uint8_t in_progress;      /* 1=正在采集中心值，约持续 320 ms */
  uint8_t valid;            /* 1=四轴中心校准有效，debug.joystick.value 可用 */
} JoystickCalibration_t;

/**
 * @brief 初始化摇杆采样功能。
 *
 * 完成 ADC1 和 ADC2 的单端硬件校准，并启动四路摇杆的中心校准流程。
 * 初始化完成后，主循环仍需周期性调用 Joystick_Update()，中心校准才会
 * 收集样本并完成。
 *
 * @return 0=成功，非 0=ADC 硬件校准失败。
 */
uint8_t Joystick_Init(void);

/**
 * @brief 重新开始四轴摇杆中心校准。
 *
 * 函数会清除上一次中心值和当前映射输出，随后由 Joystick_Update() 在
 * 32 个采样周期内（当前约 320 ms）采集各轴中位值。调用后应保持两个
 * 摇杆处于中位，直至 debug.joystick.calibration.valid 变为 1。
 */
void Joystick_StartCalibration(void);

/**
 * @brief 使用标称中心值 2048，将一路 12 位 ADC 原始值映射为 -100～+100。
 *
 * 此函数不使用实际记录的摇杆中心值，主要用于独立测试或需要固定映射
 * 的场景。正常摇杆控制应直接使用 Joystick_Update() 更新后的
 * debug.joystick.value，其中已采用每轴实际中心校准值。
 *
 * @param raw_value ADC 原始值，正常范围为 0～4095；超过 4095 时按 4095 处理。
 * @return 映射结果：0 对应 -100，2048 对应 0，4095 对应 +100。
 */
int16_t Joystick_ScaleRaw(uint16_t raw_value);

/**
 * @brief 执行一次完整的四轴摇杆采样和处理。
 *
 * 经 Board 轮询读取 ADC1 的左 Y、左 X、右 X，以及 ADC2 的右 Y；更新
 * debug.joystick.raw，再进行一阶低通滤波并更新 debug.joystick.filtered。
 * 若中心校准正在进行则累积校准样本；校准有效后，根据滤波结果和
 * 各轴中心值更新 debug.joystick.value（-100～+100）。
 *
 * @return 0=成功，非 0=ADC 启动、转换或停止失败。
 */
uint8_t Joystick_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __JOYSTICK_H__ */
