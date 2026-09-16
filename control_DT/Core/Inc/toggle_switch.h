#ifndef __TOGGLE_SWITCH_H__
#define __TOGGLE_SWITCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

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
  ToggleSwitchPosition_t switch_1;  /* 拨杆1：-1=A，1=B，2=无效 */
  ToggleSwitchPosition_t switch_2;  /* 拨杆2：-1=A，1=B，2=无效 */
  ToggleSwitchPosition_t switch_3;  /* 拨杆3：-1=A，1=B，2=无效 */
  ToggleSwitchPosition_t switch_4;  /* 拨杆4：-1=A，1=B，2=无效 */
} ToggleSwitchState_t;

/*
 * Read all eight input contacts and update switch states.
 * A new raw state must remain unchanged for three consecutive 100 Hz samples
 * (about 30 ms) before it is copied to debug.toggle_switch.state.
 */
void ToggleSwitch_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOGGLE_SWITCH_H__ */
