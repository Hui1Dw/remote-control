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
  ButtonPosition_t sw3;  /* PA6：0=释放，1=按下 */
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
  ButtonTogglePosition_t sw1;  /* SW1 锁存开/关，每次有效按下翻转，上电为关 */
  ButtonTogglePosition_t sw2;  /* SW2 锁存开/关，每次有效按下翻转，上电为关 */
  ButtonTogglePosition_t sw3;  /* SW3 锁存开/关，每次有效按下翻转，上电为关 */
  ButtonTogglePosition_t sw4;  /* SW4 锁存开/关，每次有效按下翻转，上电为关 */
} ButtonToggleState_t;

/*
 * Read all four active-low buttons and update their states.
 * A physical state must remain unchanged for three consecutive 100 Hz samples
 * (about 20 to 30 ms) before it is copied to debug.button.physical. A
 * debounced released-to-pressed transition toggles debug.button.toggle once.
 */
void Button_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H__ */
