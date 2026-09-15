#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include <stdint.h>

/* Wire format: A5 5A 01 01 SEQ 06 LY LX RY RX SW BTN CRC_L CRC_H. */
#define CONTROL_PROTOCOL_FRAME_SIZE       14U
#define CONTROL_PROTOCOL_HEADER_0         0xA5U
#define CONTROL_PROTOCOL_HEADER_1         0x5AU
#define CONTROL_PROTOCOL_VERSION          0x01U
#define CONTROL_PROTOCOL_REALTIME_TYPE    0x01U
#define CONTROL_PROTOCOL_PAYLOAD_LENGTH   0x06U

#define CONTROL_SWITCH_A                  0x01U
#define CONTROL_SWITCH_B                  0x02U

typedef struct
{
    uint8_t sequence;  /* 发送端帧序号 SEQ */
    int8_t  left_y;    /* 左摇杆 Y，有符号 -100～+100 */
    int8_t  left_x;    /* 左摇杆 X，有符号 -100～+100 */
    int8_t  right_y;   /* 右摇杆 Y，有符号 -100～+100 */
    int8_t  right_x;   /* 右摇杆 X，有符号 -100～+100 */
    uint8_t switches;  /* 拨杆打包：bit1:0=拨杆1 … bit7:6=拨杆4，01=A，10=B */
    uint8_t buttons;   /* bit0～bit3=SW1～SW4，bit4=校准有效，bit5～7 必须为 0 */
} ControlProtocol_Frame;

typedef enum
{
    CONTROL_PROTOCOL_NO_FRAME = 0,
    CONTROL_PROTOCOL_VALID_FRAME,
    CONTROL_PROTOCOL_BAD_FORMAT,
    CONTROL_PROTOCOL_BAD_CRC
} ControlProtocol_ParseResult;

typedef struct
{
    uint8_t bytes[CONTROL_PROTOCOL_FRAME_SIZE];
    uint8_t length;
} ControlProtocol_Parser;

/* CRC-16/MODBUS: initial value 0xFFFF, polynomial 0xA001. */
uint16_t ControlProtocol_Crc16Modbus(const uint8_t *data, uint16_t length);

void ControlProtocol_ParserInit(ControlProtocol_Parser *parser);
ControlProtocol_ParseResult ControlProtocol_ParserPushByte(
    ControlProtocol_Parser *parser,
    uint8_t byte,
    ControlProtocol_Frame *frame);

/* Returns 1 only when all four switch fields are A (01) or B (10). */
uint8_t ControlProtocol_SwitchesAreValid(uint8_t switches);

/* bit4 is the mandatory joystick-calibration-valid flag; bits5..7 must be 0. */
uint8_t ControlProtocol_ButtonsAreValid(uint8_t buttons);

/* All joystick axes are constrained by the on-wire protocol to -100..100. */
uint8_t ControlProtocol_AxesAreValid(const ControlProtocol_Frame *frame);

#endif /* CONTROL_PROTOCOL_H */
