/**
 * @file button.c
 * @brief Active-low momentary-button reader, debounce, and on/off latching.
 *
 * GPIO internal pull-ups keep all buttons high when released. Pressing a
 * button grounds its input and produces BUTTON_PRESSED. Each physical state
 * is debounced over three 100 Hz scans. Every debounced released-to-pressed
 * edge toggles the corresponding debug.button.toggle, so a momentary press
 * becomes a persistent on/off command for the wireless control frame.
 */
#include "button.h"
#include "debug.h"

#define BUTTON_DEBOUNCE_SAMPLES  3U

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

static ButtonPosition_t Button_Read(GPIO_TypeDef *port, uint16_t pin)
{
  return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? BUTTON_PRESSED : BUTTON_RELEASED;
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

void Button_Update(void)
{
  debug.button.raw.sw1 = Button_Read(SW1_GPIO_Port, SW1_Pin);
  debug.button.raw.sw2 = Button_Read(SW2_GPIO_Port, SW2_Pin);
  debug.button.raw.sw3 = Button_Read(SW3_GPIO_Port, SW3_Pin);
  debug.button.raw.sw4 = Button_Read(SW4_GPIO_Port, SW4_Pin);

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
  Button_ToggleOnPress(debug.button.physical.sw3,
                       &button_previous_state.sw3,
                       &debug.button.toggle.sw3);
  Button_ToggleOnPress(debug.button.physical.sw4,
                       &button_previous_state.sw4,
                       &debug.button.toggle.sw4);
}
