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

/* Reason reported when debug.safe is 1. */
#define RECEIVER_CONTROL_SAFE_REASON_NONE             0U
#define RECEIVER_CONTROL_SAFE_REASON_NO_ONLINE_SOURCE 1U
#define RECEIVER_CONTROL_SAFE_REASON_CRC_FAULT        2U
#define RECEIVER_CONTROL_SAFE_REASON_INVALID_CONTROL  3U

typedef struct
{
    int8_t  left_y;    /* 左摇杆 Y，-100～+100；安全状态为 0 */
    int8_t  left_x;    /* 左摇杆 X，-100～+100；安全状态为 0 */
    int8_t  right_y;   /* 右摇杆 Y，-100～+100；安全状态为 0 */
    int8_t  right_x;   /* 右摇杆 X，-100～+100；安全状态为 0 */
    uint8_t switches;  /* 四个拨杆打包字节，每档 2 bit：01=A，10=B */
    uint8_t buttons;   /* bit0～bit3=SW1～SW4 开关状态，bit4=校准有效 */
    uint8_t sequence;  /* 发送端帧序号 */
} ReceiverControl_Output;

void ReceiverControl_Init(void);
void ReceiverControl_PushByte(uint8_t source, uint8_t byte);
void ReceiverControl_Poll(void);

/* Called when the selected CRC-correct control state changes (not once per frame). */
void ReceiverControl_Apply(const ReceiverControl_Output *output);
/* Called on startup, timeout, CRC fault, invalid switch state, or invalid calibration. */
void ReceiverControl_ApplySafeState(void);

#endif /* RECEIVER_CONTROL_H */
