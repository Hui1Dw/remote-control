#ifndef RECEIVER_CONTROL_H
#define RECEIVER_CONTROL_H

#include <stdint.h>
#include "control_protocol.h"

#define RECEIVER_CONTROL_SOURCE_USART1  0U
#define RECEIVER_CONTROL_SOURCE_USART2  1U
#define RECEIVER_CONTROL_SOURCE_USART3  2U
#define RECEIVER_CONTROL_SOURCE_NONE    0xFFU

/* A valid control frame must be received from the selected source within this period. */
#define RECEIVER_CONTROL_LINK_TIMEOUT_MS  300U

/* Reason reported when receiver_control_safe is 1. */
#define RECEIVER_CONTROL_SAFE_REASON_NONE             0U
#define RECEIVER_CONTROL_SAFE_REASON_NO_ONLINE_SOURCE 1U
#define RECEIVER_CONTROL_SAFE_REASON_CRC_FAULT        2U
#define RECEIVER_CONTROL_SAFE_REASON_INVALID_CONTROL  3U

typedef struct
{
    int8_t  left_y;
    int8_t  left_x;
    int8_t  right_y;
    int8_t  right_x;
    uint8_t switches;
    uint8_t buttons;
    uint8_t sequence;
} ReceiverControl_Output;

void ReceiverControl_Init(void);
void ReceiverControl_PushByte(uint8_t source, uint8_t byte);
void ReceiverControl_Poll(void);

/* Called when the selected CRC-correct control state changes (not once per frame). */
void ReceiverControl_Apply(const ReceiverControl_Output *output);
/* Called on startup, timeout, CRC fault, invalid switch state, or invalid calibration. */
void ReceiverControl_ApplySafeState(void);

extern volatile ReceiverControl_Output receiver_control_output;
extern volatile uint8_t receiver_control_safe;
/* Selected candidate before safety filtering; active_source is NONE while safe. */
extern volatile uint8_t receiver_control_selected_source;
extern volatile uint8_t receiver_control_active_source;
extern volatile uint8_t receiver_control_safe_reason;
extern volatile uint8_t receiver_control_source_online[3];
/* Latest CRC-correct, fully decoded frame from each physical UART.  These
   values are retained even when that frame requests the safety state. */
extern volatile ControlProtocol_Frame receiver_control_last_frame[3];
extern volatile uint8_t receiver_control_last_frame_valid[3];
extern volatile uint8_t receiver_control_last_frame_safe[3];
/* Decoded convenience values for Watch: switch state is 1=A, 2=B;
   0 or 3 means reserved/invalid. Button state is 0=off, 1=on. */
extern volatile uint8_t receiver_control_switch_state[3][4];
extern volatile uint8_t receiver_control_button_state[3][4];
extern volatile uint8_t receiver_control_calibration_valid[3];
extern volatile uint32_t receiver_control_valid_frame_count[3];
extern volatile uint32_t receiver_control_crc_error_count[3];
extern volatile uint32_t receiver_control_format_error_count[3];
extern volatile uint32_t receiver_control_invalid_control_count[3];
extern volatile uint32_t receiver_control_safe_enter_count;

#endif /* RECEIVER_CONTROL_H */
