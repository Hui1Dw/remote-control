#include "pairing_input.h"

#include "pairing_config.h"

#include <stddef.h>

static uint8_t PairingInput_ElapsedAtLeast(uint32_t now, uint32_t then,
                                           uint32_t period)
{
  return ((uint32_t)(now - then) >= period) ? 1U : 0U;
}

static void PairingInput_ResetStage(PairingInput_t *input,
                                    PairingInputCancelReason_t reason,
                                    uint32_t now)
{
  if (input->state != PAIRING_INPUT_STATE_IDLE)
  {
    input->cancel_count++;
    if (reason == PAIRING_INPUT_CANCEL_TIMEOUT)
    {
      input->timeout_count++;
    }
  }

  input->state = PAIRING_INPUT_STATE_IDLE;
  input->last_cancel_reason = reason;
  input->first_number = 0U;
  input->second_number = 0U;
  input->last_activity_tick_ms = now;
}

static uint8_t PairingInput_AddShortPress(PairingInput_t *input,
                                          uint8_t *number,
                                          uint32_t now)
{
  if (*number >= PAIRING_CONFIG_MAX_PAIR_NUMBER)
  {
    input->overflow_count++;
    PairingInput_ResetStage(input, PAIRING_INPUT_CANCEL_NUMBER_OVERFLOW, now);
    return 0U;
  }

  (*number)++;
  input->short_press_count++;
  input->last_activity_tick_ms = now;
  return 1U;
}

void PairingInput_Init(PairingInput_t *input, uint32_t now)
{
  if (input == NULL)
  {
    return;
  }

  *input = (PairingInput_t){0};
  input->state = PAIRING_INPUT_STATE_IDLE;
  input->last_activity_tick_ms = now;
}

void PairingInput_Enter(PairingInput_t *input, uint32_t now)
{
  if (input == NULL)
  {
    return;
  }

  input->state = PAIRING_INPUT_STATE_FIRST_NUMBER;
  input->last_cancel_reason = PAIRING_INPUT_CANCEL_NONE;
  input->key_pressed = 0U;
  input->first_number = 0U;
  input->second_number = 0U;
  input->request_ready = 0U;
  input->last_activity_tick_ms = now;
  input->start_count++;
}

void PairingInput_Process(PairingInput_t *input, uint32_t now)
{
  if (input == NULL || input->state == PAIRING_INPUT_STATE_IDLE ||
      input->key_pressed != 0U)
  {
    return;
  }

  if (PairingInput_ElapsedAtLeast(now, input->last_activity_tick_ms,
                                  PAIRING_INPUT_STAGE_TIMEOUT_MS) != 0U)
  {
    PairingInput_ResetStage(input, PAIRING_INPUT_CANCEL_TIMEOUT, now);
  }
}

void PairingInput_OnPressed(PairingInput_t *input, uint32_t now)
{
  if (input == NULL || input->key_pressed != 0U)
  {
    return;
  }

  PairingInput_Process(input, now);
  input->key_pressed = 1U;
  input->press_started_tick_ms = now;
}

void PairingInput_OnReleased(PairingInput_t *input, uint32_t now)
{
  uint32_t hold_ms;

  if (input == NULL || input->key_pressed == 0U)
  {
    return;
  }

  input->key_pressed = 0U;
  hold_ms = now - input->press_started_tick_ms;

  if (input->state == PAIRING_INPUT_STATE_IDLE)
  {
    if (hold_ms >= PAIRING_INPUT_START_HOLD_MS)
    {
      PairingInput_Enter(input, now);
    }
    return;
  }

  input->last_activity_tick_ms = now;
  if (hold_ms < PAIRING_INPUT_CONFIRM_HOLD_MS)
  {
    if (input->state == PAIRING_INPUT_STATE_FIRST_NUMBER)
    {
      (void)PairingInput_AddShortPress(input, &input->first_number, now);
    }
    else
    {
      (void)PairingInput_AddShortPress(input, &input->second_number, now);
    }
    return;
  }

  if (input->state == PAIRING_INPUT_STATE_FIRST_NUMBER)
  {
    if (input->first_number < PAIRING_CONFIG_MIN_PAIR_NUMBER)
    {
      PairingInput_ResetStage(input,
                              PAIRING_INPUT_CANCEL_FIRST_NUMBER_MISSING, now);
      return;
    }

    input->state = PAIRING_INPUT_STATE_SECOND_NUMBER;
    input->second_number = 0U;
    return;
  }

  if (input->second_number < PAIRING_CONFIG_MIN_PAIR_NUMBER)
  {
    PairingInput_ResetStage(input,
                            PAIRING_INPUT_CANCEL_SECOND_NUMBER_MISSING, now);
    return;
  }

  if (input->first_number != input->second_number)
  {
    PairingInput_ResetStage(input, PAIRING_INPUT_CANCEL_NUMBER_MISMATCH, now);
    return;
  }

  input->requested_pair_number = input->first_number;
  input->request_ready = 1U;
  input->confirm_count++;
  input->state = PAIRING_INPUT_STATE_IDLE;
  input->first_number = 0U;
  input->second_number = 0U;
}

uint8_t PairingInput_TakeConfirmedNumber(PairingInput_t *input,
                                         uint8_t *pair_number)
{
  if (input == NULL || pair_number == NULL || input->request_ready == 0U)
  {
    return 0U;
  }

  *pair_number = input->requested_pair_number;
  input->request_ready = 0U;
  return 1U;
}
