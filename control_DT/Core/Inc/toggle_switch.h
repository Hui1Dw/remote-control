#ifndef __TOGGLE_SWITCH_H__
#define __TOGGLE_SWITCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * Two-position switch state. Each switch uses two active-low contacts:
 * A low / B high = A position; A high / B low = B position.
 * Both contacts high or both contacts low are invalid states.
 */
typedef enum
{
  TOGGLE_SWITCH_POSITION_A = -1,
  TOGGLE_SWITCH_POSITION_B = 1,
  TOGGLE_SWITCH_POSITION_INVALID = 2
} ToggleSwitchPosition_t;

typedef struct
{
  ToggleSwitchPosition_t switch_1;
  ToggleSwitchPosition_t switch_2;
  ToggleSwitchPosition_t switch_3;
  ToggleSwitchPosition_t switch_4;
} ToggleSwitchState_t;

/* Latest direct contact decode before debounce; useful for debug only. */
extern volatile ToggleSwitchState_t toggle_switch_raw_state;

/* Latest debounced state for the four two-position switches. */
extern volatile ToggleSwitchState_t toggle_switch_state;

/*
 * Read all eight input contacts and update switch states.
 * A new raw state must remain unchanged for three consecutive 100 Hz samples
 * (about 30 ms) before it is copied to toggle_switch_state.
 */
void ToggleSwitch_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOGGLE_SWITCH_H__ */
