#ifndef PAIRING_INPUT_H
#define PAIRING_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Pure single-button entry state machine.  The GPIO-specific debounce layer
 * supplies confirmed press/release edges; this module therefore has no HAL
 * dependency and is host-testable. */
#define PAIRING_INPUT_START_HOLD_MS       3000U
#define PAIRING_INPUT_CONFIRM_HOLD_MS     2000U
#define PAIRING_INPUT_STAGE_TIMEOUT_MS   15000U

typedef enum
{
  PAIRING_INPUT_STATE_IDLE = 0U,
  PAIRING_INPUT_STATE_FIRST_NUMBER = 1U,
  PAIRING_INPUT_STATE_SECOND_NUMBER = 2U
} PairingInputState_t;

typedef enum
{
  PAIRING_INPUT_CANCEL_NONE = 0U,
  PAIRING_INPUT_CANCEL_TIMEOUT = 1U,
  PAIRING_INPUT_CANCEL_FIRST_NUMBER_MISSING = 2U,
  PAIRING_INPUT_CANCEL_SECOND_NUMBER_MISSING = 3U,
  PAIRING_INPUT_CANCEL_NUMBER_MISMATCH = 4U,
  PAIRING_INPUT_CANCEL_NUMBER_OVERFLOW = 5U
} PairingInputCancelReason_t;

typedef struct
{
  PairingInputState_t state;
  PairingInputCancelReason_t last_cancel_reason;
  uint8_t key_pressed;
  uint8_t first_number;
  uint8_t second_number;
  uint8_t request_ready;
  uint8_t requested_pair_number;
  uint32_t press_started_tick_ms;
  uint32_t last_activity_tick_ms;
  uint32_t start_count;
  uint32_t short_press_count;
  uint32_t confirm_count;
  uint32_t cancel_count;
  uint32_t timeout_count;
  uint32_t overflow_count;
} PairingInput_t;

void PairingInput_Init(PairingInput_t *input, uint32_t now);
void PairingInput_Process(PairingInput_t *input, uint32_t now);
void PairingInput_OnPressed(PairingInput_t *input, uint32_t now);
void PairingInput_OnReleased(PairingInput_t *input, uint32_t now);

/* SW3 是业务键复用：3 s 长按已在 button 层消费。进入配对号输入不能再等
 * IDLE 长按释放，否则会把这次松开当成第一遍短按或确认。 */
void PairingInput_Enter(PairingInput_t *input, uint32_t now);

uint8_t PairingInput_TakeConfirmedNumber(PairingInput_t *input,
                                         uint8_t *pair_number);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_INPUT_H */
