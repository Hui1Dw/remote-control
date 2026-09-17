/**
 * @file button.c
 * @brief Active-low momentary-button reader, debounce, and on/off latching.
 *
 * GPIO internal pull-ups keep all buttons high when released. Pressing a
 * button grounds its input and produces BUTTON_PRESSED. Each physical state
 * is debounced over three 100 Hz scans.
 *
 * SW1/SW2/SW4 still toggle on a debounced released-to-pressed edge.
 * SW3 distinguishes short-press release (business toggle) from a continuous
 * 3 s hold (pairing enter, this press is consumed and must not toggle).
 */
#include "button.h"
#include "board.h"
#include "debug.h"

#define BUTTON_DEBOUNCE_SAMPLES   3U
#define BUTTON_SW3_LONG_PRESS_MS  3000U

static ButtonState_t button_candidate_state = {
    BUTTON_RELEASED,
    BUTTON_RELEASED,
    BUTTON_RELEASED,
    BUTTON_RELEASED};

static ButtonState_t button_previous_state = {
    BUTTON_RELEASED,
    BUTTON_RELEASED,
    BUTTON_RELEASED,
    BUTTON_RELEASED};

static uint8_t button_candidate_count[4] = {0};

static uint8_t sw3_press_active;
static uint8_t sw3_long_consumed;
static uint32_t sw3_press_start_ms;
static uint8_t sw3_capture;

static ButtonPosition_t Button_Read(uint8_t index)
{
  return (Board_ButtonIsPressed(index) != 0U) ? BUTTON_PRESSED : BUTTON_RELEASED;
}

static void Button_Debounce(ButtonPosition_t raw_state,
                            ButtonPosition_t *candidate_state,
                            uint8_t *candidate_count,
                            volatile ButtonPosition_t *stable_state)
{
  if (raw_state != *candidate_state)
  {
    *candidate_state = raw_state;
    *candidate_count = 1U;
    return;
  }

  if (*candidate_count < BUTTON_DEBOUNCE_SAMPLES)
  {
    (*candidate_count)++;
  }

  if (*candidate_count >= BUTTON_DEBOUNCE_SAMPLES)
  {
    *stable_state = *candidate_state;
  }
}

static void Button_ToggleOnPress(ButtonPosition_t stable_state,
                                 ButtonPosition_t *previous_state,
                                 volatile ButtonTogglePosition_t *toggle_state)
{
  if (*previous_state == BUTTON_RELEASED && stable_state == BUTTON_PRESSED)
  {
    *toggle_state = (*toggle_state == BUTTON_TOGGLE_OFF) ? BUTTON_TOGGLE_ON : BUTTON_TOGGLE_OFF;
  }

  *previous_state = stable_state;
}

void Button_SetSw3Capture(uint8_t enable)
{
  sw3_capture = (enable != 0U) ? 1U : 0U;
  sw3_press_active = 0U;
  sw3_long_consumed = 0U;
}

static void Button_UpdateSw3(ButtonPosition_t stable_state, uint32_t now_ms)
{
  if (sw3_capture != 0U)
  {
    button_previous_state.sw3 = stable_state;
    return;
  }

  if ((button_previous_state.sw3 == BUTTON_RELEASED) && (stable_state == BUTTON_PRESSED))
  {
    sw3_press_active = 1U;
    sw3_long_consumed = 0U;
    sw3_press_start_ms = now_ms;
  }
  else if ((sw3_press_active != 0U) && (sw3_long_consumed == 0U) &&
           (stable_state == BUTTON_PRESSED) &&
           ((uint32_t)(now_ms - sw3_press_start_ms) >= BUTTON_SW3_LONG_PRESS_MS))
  {
    sw3_long_consumed = 1U;
    debug.button.sw3_pair_enter = 1U;
    debug.button.sw3_long_count++;
  }
  else if ((button_previous_state.sw3 == BUTTON_PRESSED) && (stable_state == BUTTON_RELEASED))
  {
    if (sw3_long_consumed == 0U)
    {
      debug.button.toggle.sw3 =
          (debug.button.toggle.sw3 == BUTTON_TOGGLE_OFF) ? BUTTON_TOGGLE_ON : BUTTON_TOGGLE_OFF;
      debug.button.sw3_short_count++;
    }
    sw3_press_active = 0U;
    sw3_long_consumed = 0U;
  }

  button_previous_state.sw3 = stable_state;
}

void Button_Update(void)
{
  const uint32_t now_ms = Board_NowMs();

  debug.button.raw.sw1 = Button_Read(0U);
  debug.button.raw.sw2 = Button_Read(1U);
  debug.button.raw.sw3 = Button_Read(2U);
  debug.button.raw.sw4 = Button_Read(3U);

  Button_Debounce(debug.button.raw.sw1,
                  &button_candidate_state.sw1,
                  &button_candidate_count[0],
                  &debug.button.physical.sw1);
  Button_Debounce(debug.button.raw.sw2,
                  &button_candidate_state.sw2,
                  &button_candidate_count[1],
                  &debug.button.physical.sw2);
  Button_Debounce(debug.button.raw.sw3,
                  &button_candidate_state.sw3,
                  &button_candidate_count[2],
                  &debug.button.physical.sw3);
  Button_Debounce(debug.button.raw.sw4,
                  &button_candidate_state.sw4,
                  &button_candidate_count[3],
                  &debug.button.physical.sw4);

  Button_ToggleOnPress(debug.button.physical.sw1,
                       &button_previous_state.sw1,
                       &debug.button.toggle.sw1);
  Button_ToggleOnPress(debug.button.physical.sw2,
                       &button_previous_state.sw2,
                       &debug.button.toggle.sw2);
  Button_UpdateSw3(debug.button.physical.sw3, now_ms);
  Button_ToggleOnPress(debug.button.physical.sw4,
                       &button_previous_state.sw4,
                       &debug.button.toggle.sw4);
}
