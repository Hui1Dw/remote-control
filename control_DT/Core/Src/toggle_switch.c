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

#define TOGGLE_SWITCH_DEBOUNCE_SAMPLES  3U

volatile ToggleSwitchState_t toggle_switch_raw_state = {
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID};

volatile ToggleSwitchState_t toggle_switch_state = {
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID};

static ToggleSwitchState_t toggle_switch_candidate_state = {
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID,
    TOGGLE_SWITCH_POSITION_INVALID};

static uint8_t toggle_switch_candidate_count[4] = {0};

static ToggleSwitchPosition_t ToggleSwitch_ReadPosition(GPIO_TypeDef *a_port,
                                                         uint16_t a_pin,
                                                         GPIO_TypeDef *b_port,
                                                         uint16_t b_pin)
{
  GPIO_PinState a_state = HAL_GPIO_ReadPin(a_port, a_pin);
  GPIO_PinState b_state = HAL_GPIO_ReadPin(b_port, b_pin);

  if (a_state == GPIO_PIN_RESET && b_state == GPIO_PIN_SET)
  {
    return TOGGLE_SWITCH_POSITION_A;
  }

  if (a_state == GPIO_PIN_SET && b_state == GPIO_PIN_RESET)
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
  toggle_switch_raw_state.switch_1 = ToggleSwitch_ReadPosition(SWITCH_A_1_GPIO_Port,
                                                                 SWITCH_A_1_Pin,
                                                                 SWITCH_B_1_GPIO_Port,
                                                                 SWITCH_B_1_Pin);
  toggle_switch_raw_state.switch_2 = ToggleSwitch_ReadPosition(SWITCH_A_2_GPIO_Port,
                                                                 SWITCH_A_2_Pin,
                                                                 SWITCH_B_2_GPIO_Port,
                                                                 SWITCH_B_2_Pin);
  toggle_switch_raw_state.switch_3 = ToggleSwitch_ReadPosition(SWITCH_A_3_GPIO_Port,
                                                                 SWITCH_A_3_Pin,
                                                                 SWITCH_B_3_GPIO_Port,
                                                                 SWITCH_B_3_Pin);
  toggle_switch_raw_state.switch_4 = ToggleSwitch_ReadPosition(SWITCH_A_4_GPIO_Port,
                                                                 SWITCH_A_4_Pin,
                                                                 SWITCH_B_4_GPIO_Port,
                                                                 SWITCH_B_4_Pin);

  ToggleSwitch_Debounce(toggle_switch_raw_state.switch_1,
                        &toggle_switch_candidate_state.switch_1,
                        &toggle_switch_candidate_count[0],
                        &toggle_switch_state.switch_1);
  ToggleSwitch_Debounce(toggle_switch_raw_state.switch_2,
                        &toggle_switch_candidate_state.switch_2,
                        &toggle_switch_candidate_count[1],
                        &toggle_switch_state.switch_2);
  ToggleSwitch_Debounce(toggle_switch_raw_state.switch_3,
                        &toggle_switch_candidate_state.switch_3,
                        &toggle_switch_candidate_count[2],
                        &toggle_switch_state.switch_3);
  ToggleSwitch_Debounce(toggle_switch_raw_state.switch_4,
                        &toggle_switch_candidate_state.switch_4,
                        &toggle_switch_candidate_count[3],
                        &toggle_switch_state.switch_4);
}
