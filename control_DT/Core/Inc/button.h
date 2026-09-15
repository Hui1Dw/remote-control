#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Buttons use active-low inputs: released = high, pressed = low. */
typedef enum
{
  BUTTON_RELEASED = 0,
  BUTTON_PRESSED = 1
} ButtonPosition_t;

typedef struct
{
  ButtonPosition_t sw1;
  ButtonPosition_t sw2;
  ButtonPosition_t sw3;
  ButtonPosition_t sw4;
} ButtonState_t;

/* Logical on/off state toggled by each debounced button press. */
typedef enum
{
  BUTTON_TOGGLE_OFF = 0,
  BUTTON_TOGGLE_ON = 1
} ButtonTogglePosition_t;

typedef struct
{
  ButtonTogglePosition_t sw1;
  ButtonTogglePosition_t sw2;
  ButtonTogglePosition_t sw3;
  ButtonTogglePosition_t sw4;
} ButtonToggleState_t;

/* Latest direct GPIO reading before debounce; useful for debugging. */
extern volatile ButtonState_t button_raw_state;

/* Latest debounced physical state of SW1 through SW4. */
extern volatile ButtonState_t button_state;

/*
 * Latched business state of SW1 through SW4. Each debounced press edge
 * toggles the corresponding state; all states start in BUTTON_TOGGLE_OFF.
 */
extern volatile ButtonToggleState_t button_toggle_state;

/*
 * Read all four active-low buttons and update their states.
 * A physical state must remain unchanged for three consecutive 100 Hz samples
 * (about 20 to 30 ms) before it is copied to button_state. A debounced
 * released-to-pressed transition toggles button_toggle_state once.
 */
void Button_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H__ */
