/**
 * @file toggle_switch.c
 * @brief Four active-low two-position switch input decoder with debounce.
 *
 * Every switch has an A contact and a B contact with MCU internal pull-ups.
 * Position A grounds the A contact and position B grounds the B contact.
 * Both contacts high or both contacts low are treated as invalid wiring or
 * transition states.
 *
 * Each newly decoded state must be observed in three consecutive 100 Hz scans
 * (about 30 ms) before it becomes the public debounced switch state.
 */
#include "toggle_switch.h"
#include "board.h"
#include "debug.h"

#define TOGGLE_SWITCH_DEBOUNCE_SAMPLES  3U

static ToggleSwitchState_t toggle_switch_candidate_state = {
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID};

static uint8_t toggle_switch_candidate_count[4] = {0};

static ToggleSwitchPosition_t ToggleSwitch_ReadPosition(uint8_t index)
{
  uint8_t a_low = Board_ToggleAIsLow(index);
  uint8_t b_low = Board_ToggleBIsLow(index);

  if (a_low != 0U && b_low == 0U)
  {
    return TOGGLE_SWITCH_POSITION_A;
  }

  if (a_low == 0U && b_low != 0U)
  {
    return TOGGLE_SWITCH_POSITION_B;
  }

  return TOGGLE_SWITCH_POSITION_INVALID;
}

/*
 * Accept a raw state only after it remains unchanged for three scans.
 * At the current 100 Hz update rate this rejects contact bounce shorter than
 * about 30 ms, while keeping the previous stable switch position during it.
 */
static void ToggleSwitch_Debounce(ToggleSwitchPosition_t raw_state,
                                  ToggleSwitchPosition_t *candidate_state,
                                  uint8_t *candidate_count,
                                  volatile ToggleSwitchPosition_t *stable_state)
{
  if (raw_state != *candidate_state)
  {
    *candidate_state = raw_state;
    *candidate_count = 1U;
    return;
  }

  if (*candidate_count < TOGGLE_SWITCH_DEBOUNCE_SAMPLES)
  {
    (*candidate_count)++;
  }

  if (*candidate_count >= TOGGLE_SWITCH_DEBOUNCE_SAMPLES)
  {
    *stable_state = *candidate_state;
  }
}

void ToggleSwitch_Update(void)
{
  debug.toggle_switch.raw.switch_1 = ToggleSwitch_ReadPosition(0U);
  debug.toggle_switch.raw.switch_2 = ToggleSwitch_ReadPosition(1U);
  debug.toggle_switch.raw.switch_3 = ToggleSwitch_ReadPosition(2U);
  debug.toggle_switch.raw.switch_4 = ToggleSwitch_ReadPosition(3U);

  ToggleSwitch_Debounce(debug.toggle_switch.raw.switch_1,
                        &toggle_switch_candidate_state.switch_1,
                        &toggle_switch_candidate_count[0],
                        &debug.toggle_switch.state.switch_1);
  ToggleSwitch_Debounce(debug.toggle_switch.raw.switch_2,
                        &toggle_switch_candidate_state.switch_2,
                        &toggle_switch_candidate_count[1],
                        &debug.toggle_switch.state.switch_2);
  ToggleSwitch_Debounce(debug.toggle_switch.raw.switch_3,
                        &toggle_switch_candidate_state.switch_3,
                        &toggle_switch_candidate_count[2],
                        &debug.toggle_switch.state.switch_3);
  ToggleSwitch_Debounce(debug.toggle_switch.raw.switch_4,
                        &toggle_switch_candidate_state.switch_4,
                        &toggle_switch_candidate_count[3],
                        &debug.toggle_switch.state.switch_4);
}
