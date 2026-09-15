#include "control_protocol.h"

uint16_t ControlProtocol_Crc16Modbus(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    if (data == 0)
    {
        return crc;
    }

    for (i = 0U; i < length; ++i)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

void ControlProtocol_ParserInit(ControlProtocol_Parser *parser)
{
    if (parser != 0)
    {
        parser->length = 0U;
    }
}

static void ControlProtocol_ParserRestart(ControlProtocol_Parser *parser,
                                           uint8_t byte)
{
    parser->length = 0U;
    if (byte == CONTROL_PROTOCOL_HEADER_0)
    {
        parser->bytes[parser->length++] = byte;
    }
}

ControlProtocol_ParseResult ControlProtocol_ParserPushByte(
    ControlProtocol_Parser *parser,
    uint8_t byte,
    ControlProtocol_Frame *frame)
{
    uint16_t received_crc;
    uint16_t calculated_crc;

    if (parser == 0 || frame == 0)
    {
        return CONTROL_PROTOCOL_BAD_FORMAT;
    }

    if (parser->length == 0U)
    {
        if (byte == CONTROL_PROTOCOL_HEADER_0)
        {
            parser->bytes[parser->length++] = byte;
        }
        return CONTROL_PROTOCOL_NO_FRAME;
    }

    if (parser->length == 1U)
    {
        if (byte == CONTROL_PROTOCOL_HEADER_1)
        {
            parser->bytes[parser->length++] = byte;
        }
        else
        {
            ControlProtocol_ParserRestart(parser, byte);
        }
        return CONTROL_PROTOCOL_NO_FRAME;
    }

    parser->bytes[parser->length++] = byte;

    /* Reject an invalid fixed header as early as possible and keep scanning. */
    if ((parser->length == 3U && parser->bytes[2] != CONTROL_PROTOCOL_VERSION) ||
        (parser->length == 4U && parser->bytes[3] != CONTROL_PROTOCOL_REALTIME_TYPE) ||
        (parser->length == 6U && parser->bytes[5] != CONTROL_PROTOCOL_PAYLOAD_LENGTH))
    {
        ControlProtocol_ParserRestart(parser, byte);
        return CONTROL_PROTOCOL_BAD_FORMAT;
    }

    if (parser->length < CONTROL_PROTOCOL_FRAME_SIZE)
    {
        return CONTROL_PROTOCOL_NO_FRAME;
    }

    received_crc = (uint16_t)parser->bytes[12] |
                   ((uint16_t)parser->bytes[13] << 8U);
    calculated_crc = ControlProtocol_Crc16Modbus(&parser->bytes[2], 10U);

    if (received_crc != calculated_crc)
    {
        ControlProtocol_ParserRestart(parser, byte);
        return CONTROL_PROTOCOL_BAD_CRC;
    }

    frame->sequence = parser->bytes[4];
    frame->left_y   = (int8_t)parser->bytes[6];
    frame->left_x   = (int8_t)parser->bytes[7];
    frame->right_y  = (int8_t)parser->bytes[8];
    frame->right_x  = (int8_t)parser->bytes[9];
    frame->switches = parser->bytes[10];
    frame->buttons  = parser->bytes[11];
    parser->length = 0U;

    return CONTROL_PROTOCOL_VALID_FRAME;
}

uint8_t ControlProtocol_SwitchesAreValid(uint8_t switches)
{
    uint8_t index;

    for (index = 0U; index < 4U; ++index)
    {
        uint8_t state = (uint8_t)((switches >> (index * 2U)) & 0x03U);
        if (state != CONTROL_SWITCH_A && state != CONTROL_SWITCH_B)
        {
            return 0U;
        }
    }

    return 1U;
}

uint8_t ControlProtocol_ButtonsAreValid(uint8_t buttons)
{
    return ((buttons & 0xF0U) == 0x10U) ? 1U : 0U;
}

uint8_t ControlProtocol_AxesAreValid(const ControlProtocol_Frame *frame)
{
    if (frame == 0)
    {
        return 0U;
    }

    return (frame->left_y >= -100 && frame->left_y <= 100 &&
            frame->left_x >= -100 && frame->left_x <= 100 &&
            frame->right_y >= -100 && frame->right_y <= 100 &&
            frame->right_x >= -100 && frame->right_x <= 100) ? 1U : 0U;
}
