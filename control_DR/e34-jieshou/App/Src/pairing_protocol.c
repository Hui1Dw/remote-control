#include "pairing_protocol.h"

#include "pairing_config.h"

#include <stddef.h>

#define PAIRING_PROTOCOL_OFFSET_MAGIC       0U
#define PAIRING_PROTOCOL_OFFSET_VERSION     1U
#define PAIRING_PROTOCOL_OFFSET_COMMAND     2U
#define PAIRING_PROTOCOL_OFFSET_SESSION     3U
#define PAIRING_PROTOCOL_OFFSET_PAIR_NUMBER 7U
#define PAIRING_PROTOCOL_OFFSET_TX_ADDRESS  8U
#define PAIRING_PROTOCOL_OFFSET_RX_ADDRESS 10U
#define PAIRING_PROTOCOL_OFFSET_RESERVED   12U

static uint32_t PairingProtocol_Crc32(const uint8_t *data, uint32_t length)
{
  uint32_t crc = 0xFFFFFFFFU;
  uint32_t index;
  uint8_t bit;

  for (index = 0U; index < length; index++)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
    }
  }

  return ~crc;
}

static uint8_t PairingProtocol_CommandIsValid(uint8_t command)
{
  return (command >= (uint8_t)PAIRING_PROTOCOL_COMMAND_PREPARE &&
          command <= (uint8_t)PAIRING_PROTOCOL_COMMAND_OK) ? 1U : 0U;
}

static void PairingProtocol_WriteU32Be(uint8_t *output, uint32_t value)
{
  output[0] = (uint8_t)(value >> 24U);
  output[1] = (uint8_t)(value >> 16U);
  output[2] = (uint8_t)(value >> 8U);
  output[3] = (uint8_t)value;
}

static uint32_t PairingProtocol_ReadU32Be(const uint8_t *input)
{
  return ((uint32_t)input[0] << 24U) |
         ((uint32_t)input[1] << 16U) |
         ((uint32_t)input[2] << 8U) |
         (uint32_t)input[3];
}

static uint16_t PairingProtocol_ReadU16Be(const uint8_t *input)
{
  return (uint16_t)(((uint16_t)input[0] << 8U) | input[1]);
}

static uint8_t PairingProtocol_FrameFieldsAreValid(const PairingProtocolFrame_t *frame)
{
  uint16_t expected_transmitter_address;
  uint16_t expected_receiver_address;

  if (frame == NULL || frame->session_id == 0U ||
      PairingProtocol_CommandIsValid((uint8_t)frame->command) == 0U ||
      PairingConfig_Derive(frame->pair_number, &expected_transmitter_address,
                           &expected_receiver_address) == 0U ||
      frame->transmitter_address != expected_transmitter_address ||
      frame->receiver_address != expected_receiver_address)
  {
    return 0U;
  }

  return 1U;
}

uint8_t PairingProtocol_Build(const PairingProtocolFrame_t *frame,
                              uint8_t output[PAIRING_PROTOCOL_FRAME_SIZE])
{
  uint32_t crc;

  if (output == NULL || PairingProtocol_FrameFieldsAreValid(frame) == 0U)
  {
    return 0U;
  }

  output[PAIRING_PROTOCOL_OFFSET_MAGIC] = PAIRING_PROTOCOL_MAGIC;
  output[PAIRING_PROTOCOL_OFFSET_VERSION] = PAIRING_PROTOCOL_VERSION;
  output[PAIRING_PROTOCOL_OFFSET_COMMAND] = (uint8_t)frame->command;
  PairingProtocol_WriteU32Be(&output[PAIRING_PROTOCOL_OFFSET_SESSION],
                             frame->session_id);
  output[PAIRING_PROTOCOL_OFFSET_PAIR_NUMBER] = frame->pair_number;
  output[PAIRING_PROTOCOL_OFFSET_TX_ADDRESS] =
      (uint8_t)(frame->transmitter_address >> 8U);
  output[PAIRING_PROTOCOL_OFFSET_TX_ADDRESS + 1U] =
      (uint8_t)frame->transmitter_address;
  output[PAIRING_PROTOCOL_OFFSET_RX_ADDRESS] =
      (uint8_t)(frame->receiver_address >> 8U);
  output[PAIRING_PROTOCOL_OFFSET_RX_ADDRESS + 1U] =
      (uint8_t)frame->receiver_address;
  output[PAIRING_PROTOCOL_OFFSET_RESERVED] = 0U;

  crc = PairingProtocol_Crc32(output, PAIRING_PROTOCOL_CRC_OFFSET);
  PairingProtocol_WriteU32Be(&output[PAIRING_PROTOCOL_CRC_OFFSET], crc);
  return 1U;
}

uint8_t PairingProtocol_Parse(const uint8_t input[PAIRING_PROTOCOL_FRAME_SIZE],
                              PairingProtocolFrame_t *frame)
{
  PairingProtocolFrame_t decoded;
  uint32_t expected_crc;

  if (input == NULL || frame == NULL ||
      input[PAIRING_PROTOCOL_OFFSET_MAGIC] != PAIRING_PROTOCOL_MAGIC ||
      input[PAIRING_PROTOCOL_OFFSET_VERSION] != PAIRING_PROTOCOL_VERSION ||
      input[PAIRING_PROTOCOL_OFFSET_RESERVED] != 0U ||
      PairingProtocol_CommandIsValid(input[PAIRING_PROTOCOL_OFFSET_COMMAND]) == 0U)
  {
    return 0U;
  }

  expected_crc = PairingProtocol_Crc32(input, PAIRING_PROTOCOL_CRC_OFFSET);
  if (PairingProtocol_ReadU32Be(&input[PAIRING_PROTOCOL_CRC_OFFSET]) != expected_crc)
  {
    return 0U;
  }

  decoded.command = (PairingProtocolCommand_t)input[PAIRING_PROTOCOL_OFFSET_COMMAND];
  decoded.session_id = PairingProtocol_ReadU32Be(&input[PAIRING_PROTOCOL_OFFSET_SESSION]);
  decoded.pair_number = input[PAIRING_PROTOCOL_OFFSET_PAIR_NUMBER];
  decoded.transmitter_address =
      PairingProtocol_ReadU16Be(&input[PAIRING_PROTOCOL_OFFSET_TX_ADDRESS]);
  decoded.receiver_address =
      PairingProtocol_ReadU16Be(&input[PAIRING_PROTOCOL_OFFSET_RX_ADDRESS]);

  if (PairingProtocol_FrameFieldsAreValid(&decoded) == 0U)
  {
    return 0U;
  }

  *frame = decoded;
  return 1U;
}
