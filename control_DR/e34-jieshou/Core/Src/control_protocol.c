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


uint8_t ControlProtocol_Crc8Maxim(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint8_t bit;
    uint8_t crc = 0x00U;

    if (data == 0)
    {
        return crc;
    }

    for (i = 0U; i < length; ++i)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x01U) != 0U)
            {
                crc = (uint8_t)((crc >> 1U) ^ 0x8CU);
            }
            else
            {
                crc = (uint8_t)(crc >> 1U);
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
    uint8_t received_crc;
    uint8_t calculated_crc;

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

    /* Second byte is VER:4 SEQ:4. A wrong version is not a committed frame. */
    if (parser->length == 1U)
    {
        if ((uint8_t)(byte >> 4U) == CONTROL_PROTOCOL_VERSION)
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

    if (parser->length < CONTROL_PROTOCOL_FRAME_SIZE)
    {
        return CONTROL_PROTOCOL_NO_FRAME;
    }

    received_crc = parser->bytes[CONTROL_PROTOCOL_CRC_OFFSET];
    calculated_crc = ControlProtocol_Crc8Maxim(&parser->bytes[1],
                                               CONTROL_PROTOCOL_CRC_LENGTH);

    if (received_crc != calculated_crc)
    {
        ControlProtocol_ParserRestart(parser, byte);
        return CONTROL_PROTOCOL_BAD_CRC;
    }

    frame->sequence = (uint8_t)(parser->bytes[1] & CONTROL_PROTOCOL_SEQUENCE_MASK);
    frame->left_y   = (int8_t)parser->bytes[2];
    frame->left_x   = (int8_t)parser->bytes[3];
    frame->right_y  = (int8_t)parser->bytes[4];
    frame->right_x  = (int8_t)parser->bytes[5];
    frame->switches = parser->bytes[6];
    frame->buttons  = parser->bytes[7];
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
