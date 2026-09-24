#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Buttons use active-low inputs: released = high, pressed = low. */
typedef enum
{
  BUTTON_RELEASED = 0,
  BUTTON_PRESSED = 1
} ButtonPosition_t;

typedef struct
{
  ButtonPosition_t sw1;  /* PA3，右摇杆按键：0=释放，1=按下 */
  ButtonPosition_t sw2;  /* PA5，左摇杆按键：0=释放，1=按下 */
  ButtonPosition_t sw3;  /* PA6：0=释放，1=按下；短按/长按复用配对键 */
  ButtonPosition_t sw4;  /* PA7：0=释放，1=按下 */
} ButtonState_t;

/* Logical on/off state toggled by each debounced button press. */
typedef enum
{
  BUTTON_TOGGLE_OFF = 0,
  BUTTON_TOGGLE_ON = 1
} ButtonTogglePosition_t;

typedef struct
{
  ButtonTogglePosition_t sw1;  /* SW1 锁存开/关，按下边沿翻转，上电为关 */
  ButtonTogglePosition_t sw2;  /* SW2 锁存开/关，按下边沿翻转，上电为关 */
  ButtonTogglePosition_t sw3;  /* SW3 锁存开/关，短按释放翻转；长按 3s 不翻转 */
  ButtonTogglePosition_t sw4;  /* SW4 锁存开/关，按下边沿翻转，上电为关 */
} ButtonToggleState_t;

/*
 * Read all four active-low buttons and update their states.
 * Debounce is unchanged: three consecutive 100 Hz samples before physical.
 * SW1/SW2/SW4 still toggle on a debounced released-to-pressed edge.
 * SW3 is an event state machine: short-press release toggles; a continuous
 * 3 s hold requests pairing and consumes the press so toggle does not flip.
 */
void Button_Update(void);

/**
 * @brief 配对号输入期间接管 SW3。
 *
 * enable=1：仍更新 raw/physical 和消抖，但不再翻转 toggle，也不再产生
 * sw3_pair_enter。enable=0：恢复短按翻转 / 长按进入配对。
 */
void Button_SetSw3Capture(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H__ */
