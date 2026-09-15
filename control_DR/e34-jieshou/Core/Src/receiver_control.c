#include "receiver_control.h"
#include "stm32g4xx_hal.h"

/* Existing emergency receiver priority is retained: USART2 > USART1 > USART3. */
static const uint8_t receiver_control_priority[3] = {
    RECEIVER_CONTROL_SOURCE_USART2,
    RECEIVER_CONTROL_SOURCE_USART1,
    RECEIVER_CONTROL_SOURCE_USART3
};

typedef struct
{
    ControlProtocol_Parser parser;
    ControlProtocol_Frame frame;
    uint32_t last_valid_tick;
    uint8_t has_valid_frame;
    uint8_t unsafe_frame_received;
    uint8_t crc_fault;
} ReceiverControl_Source;

static ReceiverControl_Source receiver_control_sources[3];

volatile ReceiverControl_Output receiver_control_output = {0};
volatile uint8_t receiver_control_safe = 1U;
volatile uint8_t receiver_control_selected_source = RECEIVER_CONTROL_SOURCE_NONE;
volatile uint8_t receiver_control_active_source = RECEIVER_CONTROL_SOURCE_NONE;
volatile uint8_t receiver_control_safe_reason = RECEIVER_CONTROL_SAFE_REASON_NO_ONLINE_SOURCE;
volatile uint8_t receiver_control_source_online[3] = {0U, 0U, 0U};
volatile ControlProtocol_Frame receiver_control_last_frame[3] = {{0}};
volatile uint8_t receiver_control_last_frame_valid[3] = {0U, 0U, 0U};
volatile uint8_t receiver_control_last_frame_safe[3] = {0U, 0U, 0U};
volatile uint8_t receiver_control_switch_state[3][4] = {{0U}};
volatile uint8_t receiver_control_button_state[3][4] = {{0U}};
volatile uint8_t receiver_control_calibration_valid[3] = {0U, 0U, 0U};
volatile uint32_t receiver_control_valid_frame_count[3] = {0U, 0U, 0U};
volatile uint32_t receiver_control_crc_error_count[3] = {0U, 0U, 0U};
volatile uint32_t receiver_control_format_error_count[3] = {0U, 0U, 0U};
volatile uint32_t receiver_control_invalid_control_count[3] = {0U, 0U, 0U};
volatile uint32_t receiver_control_safe_enter_count = 0U;

static uint8_t ReceiverControl_SourceIsOnline(uint8_t source, uint32_t now)
{
    ReceiverControl_Source *entry;

    if (source > RECEIVER_CONTROL_SOURCE_USART3)
    {
        return 0U;
    }

    entry = &receiver_control_sources[source];
    return (entry->has_valid_frame != 0U &&
            (uint32_t)(now - entry->last_valid_tick) <=
                RECEIVER_CONTROL_LINK_TIMEOUT_MS) ? 1U : 0U;
}

static void ReceiverControl_SetSafeState(uint8_t reason)
{
    if (receiver_control_safe == 0U)
    {
        receiver_control_safe_enter_count++;
        ReceiverControl_ApplySafeState();
    }

    receiver_control_safe = 1U;
    receiver_control_safe_reason = reason;
    receiver_control_active_source = RECEIVER_CONTROL_SOURCE_NONE;
    receiver_control_output.left_y = 0;
    receiver_control_output.left_x = 0;
    receiver_control_output.right_y = 0;
    receiver_control_output.right_x = 0;
    receiver_control_output.switches = 0U;
    receiver_control_output.buttons = 0U;
    receiver_control_output.sequence = 0U;
}

static uint8_t ReceiverControl_OutputStateEquals(
    const ReceiverControl_Output *left,
    const ReceiverControl_Output *right)
{
    return (left->left_y == right->left_y &&
            left->left_x == right->left_x &&
            left->right_y == right->right_y &&
            left->right_x == right->right_x &&
            left->switches == right->switches &&
            left->buttons == right->buttons) ? 1U : 0U;
}

static void ReceiverControl_SetOutput(uint8_t source,
                                      const ControlProtocol_Frame *frame)
{
    ReceiverControl_Output output;
    uint8_t output_changed;

    output.left_y = frame->left_y;
    output.left_x = frame->left_x;
    output.right_y = frame->right_y;
    output.right_x = frame->right_x;
    output.switches = frame->switches;
    output.buttons = frame->buttons;
    output.sequence = frame->sequence;

    output_changed = (receiver_control_safe != 0U ||
                      receiver_control_active_source != source ||
                      ReceiverControl_OutputStateEquals(
                          (const ReceiverControl_Output *)&receiver_control_output,
                          &output) == 0U) ? 1U : 0U;

    /* Sequence is retained for diagnostics even if the control state is unchanged. */
    receiver_control_output = output;
    receiver_control_selected_source = source;
    receiver_control_active_source = source;
    receiver_control_safe = 0U;
    receiver_control_safe_reason = RECEIVER_CONTROL_SAFE_REASON_NONE;

    /* State outputs are idempotent: a held button does not cause a repeated action. */
    if (output_changed != 0U)
    {
        ReceiverControl_Apply(&output);
    }
}

void ReceiverControl_Init(void)
{
    uint8_t source;

    for (source = 0U; source < 3U; ++source)
    {
        ControlProtocol_ParserInit(&receiver_control_sources[source].parser);
        receiver_control_sources[source].has_valid_frame = 0U;
        receiver_control_sources[source].unsafe_frame_received = 0U;
        receiver_control_sources[source].crc_fault = 0U;
        receiver_control_source_online[source] = 0U;
        receiver_control_last_frame[source].sequence = 0U;
        receiver_control_last_frame[source].left_y = 0;
        receiver_control_last_frame[source].left_x = 0;
        receiver_control_last_frame[source].right_y = 0;
        receiver_control_last_frame[source].right_x = 0;
        receiver_control_last_frame[source].switches = 0U;
        receiver_control_last_frame[source].buttons = 0U;
        receiver_control_last_frame_valid[source] = 0U;
        receiver_control_last_frame_safe[source] = 0U;
        receiver_control_switch_state[source][0] = 0U;
        receiver_control_switch_state[source][1] = 0U;
        receiver_control_switch_state[source][2] = 0U;
        receiver_control_switch_state[source][3] = 0U;
        receiver_control_button_state[source][0] = 0U;
        receiver_control_button_state[source][1] = 0U;
        receiver_control_button_state[source][2] = 0U;
        receiver_control_button_state[source][3] = 0U;
        receiver_control_calibration_valid[source] = 0U;
    }

    receiver_control_safe = 0U;
    receiver_control_selected_source = RECEIVER_CONTROL_SOURCE_NONE;
    ReceiverControl_SetSafeState(RECEIVER_CONTROL_SAFE_REASON_NO_ONLINE_SOURCE);
}

void ReceiverControl_PushByte(uint8_t source, uint8_t byte)
{
    ControlProtocol_Frame frame;
    ControlProtocol_ParseResult result;
    ReceiverControl_Source *entry;

    if (source > RECEIVER_CONTROL_SOURCE_USART3)
    {
        return;
    }

    entry = &receiver_control_sources[source];
    result = ControlProtocol_ParserPushByte(&entry->parser, byte, &frame);

    if (result == CONTROL_PROTOCOL_BAD_CRC)
    {
        receiver_control_crc_error_count[source]++;
        /* A CRC error remains unsafe until this channel sends a new valid frame. */
        entry->crc_fault = 1U;
        return;
    }

    if (result == CONTROL_PROTOCOL_BAD_FORMAT)
    {
        receiver_control_format_error_count[source]++;
        return;
    }

    if (result != CONTROL_PROTOCOL_VALID_FRAME)
    {
        return;
    }

    entry->frame = frame;
    entry->last_valid_tick = HAL_GetTick();
    entry->has_valid_frame = 1U;
    entry->crc_fault = 0U;
    receiver_control_valid_frame_count[source]++;

    entry->unsafe_frame_received =
        (ControlProtocol_SwitchesAreValid(frame.switches) == 0U ||
         ControlProtocol_ButtonsAreValid(frame.buttons) == 0U ||
         ControlProtocol_AxesAreValid(&frame) == 0U) ? 1U : 0U;

    /* Expose every CRC-correct decoded frame per UART for debugger viewing.
       Do this before the safety decision so invalid switch/calibration states
       can be diagnosed instead of being hidden by the zeroed final output. */
    receiver_control_last_frame[source].sequence = frame.sequence;
    receiver_control_last_frame[source].left_y = frame.left_y;
    receiver_control_last_frame[source].left_x = frame.left_x;
    receiver_control_last_frame[source].right_y = frame.right_y;
    receiver_control_last_frame[source].right_x = frame.right_x;
    receiver_control_last_frame[source].switches = frame.switches;
    receiver_control_last_frame[source].buttons = frame.buttons;
    receiver_control_last_frame_valid[source] = 1U;
    receiver_control_last_frame_safe[source] =
        (entry->unsafe_frame_received == 0U) ? 1U : 0U;
    receiver_control_switch_state[source][0] = (uint8_t)(frame.switches & 0x03U);
    receiver_control_switch_state[source][1] = (uint8_t)((frame.switches >> 2U) & 0x03U);
    receiver_control_switch_state[source][2] = (uint8_t)((frame.switches >> 4U) & 0x03U);
    receiver_control_switch_state[source][3] = (uint8_t)((frame.switches >> 6U) & 0x03U);
    receiver_control_button_state[source][0] = (uint8_t)(frame.buttons & 0x01U);
    receiver_control_button_state[source][1] = (uint8_t)((frame.buttons >> 1U) & 0x01U);
    receiver_control_button_state[source][2] = (uint8_t)((frame.buttons >> 2U) & 0x01U);
    receiver_control_button_state[source][3] = (uint8_t)((frame.buttons >> 3U) & 0x01U);
    receiver_control_calibration_valid[source] =
        (uint8_t)((frame.buttons >> 4U) & 0x01U);

    if (entry->unsafe_frame_received != 0U)
    {
        receiver_control_invalid_control_count[source]++;
    }
}

void ReceiverControl_Poll(void)
{
    uint8_t index;
    uint8_t selected = RECEIVER_CONTROL_SOURCE_NONE;
    uint32_t now = HAL_GetTick();

    for (index = 0U; index < 3U; ++index)
    {
        receiver_control_source_online[index] =
            ReceiverControl_SourceIsOnline(index, now);
    }

    for (index = 0U; index < 3U; ++index)
    {
        uint8_t candidate = receiver_control_priority[index];
        if (receiver_control_source_online[candidate] != 0U)
        {
            selected = candidate;
            break;
        }
    }

    receiver_control_selected_source = selected;

    if (selected == RECEIVER_CONTROL_SOURCE_NONE)
    {
        ReceiverControl_SetSafeState(RECEIVER_CONTROL_SAFE_REASON_NO_ONLINE_SOURCE);
        return;
    }

    /* Do not fall through to a lower-priority link after a selected-link fault. */
    if (receiver_control_sources[selected].crc_fault != 0U)
    {
        ReceiverControl_SetSafeState(RECEIVER_CONTROL_SAFE_REASON_CRC_FAULT);
        return;
    }

    if (receiver_control_sources[selected].unsafe_frame_received != 0U)
    {
        ReceiverControl_SetSafeState(RECEIVER_CONTROL_SAFE_REASON_INVALID_CONTROL);
        return;
    }

    ReceiverControl_SetOutput(selected, &receiver_control_sources[selected].frame);
}

/* Default hooks keep this module board-independent.  The application may replace them. */
__weak void ReceiverControl_Apply(const ReceiverControl_Output *output)
{
    (void)output;
}

__weak void ReceiverControl_ApplySafeState(void)
{
}
