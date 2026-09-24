#ifndef PAIRING_PROTOCOL_H
#define PAIRING_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Pairing traffic is deliberately distinct from the 9-byte 0xA5 control
 * protocol.  These are E22 application payload bytes; fixed-transfer address
 * and channel bytes remain outside this frame. */
#define PAIRING_PROTOCOL_MAGIC             ((uint8_t)0xD4U)
#define PAIRING_PROTOCOL_VERSION           ((uint8_t)1U)
#define PAIRING_PROTOCOL_FRAME_SIZE        ((uint8_t)17U)
#define PAIRING_PROTOCOL_CRC_OFFSET        ((uint8_t)13U)

typedef enum
{
  PAIRING_PROTOCOL_COMMAND_PREPARE = 1U,
  PAIRING_PROTOCOL_COMMAND_PREPARE_ACK = 2U,
  PAIRING_PROTOCOL_COMMAND_COMMITTED = 3U,
  PAIRING_PROTOCOL_COMMAND_VERIFY = 4U,
  PAIRING_PROTOCOL_COMMAND_OK = 5U
} PairingProtocolCommand_t;

typedef struct
{
  PairingProtocolCommand_t command;
  uint32_t session_id;
  uint8_t pair_number;
  uint16_t transmitter_address;
  uint16_t receiver_address;
} PairingProtocolFrame_t;

/* Build a CRC32-protected pairing frame. The address fields must match the
 * deterministic mapping for pair_number; this prevents a malformed input from
 * selecting arbitrary E22 identities. */
uint8_t PairingProtocol_Build(const PairingProtocolFrame_t *frame,
                              uint8_t output[PAIRING_PROTOCOL_FRAME_SIZE]);

/* Validate and decode one complete pairing payload. This performs framing,
 * version, command, CRC32 and deterministic-address validation. */
uint8_t PairingProtocol_Parse(const uint8_t input[PAIRING_PROTOCOL_FRAME_SIZE],
                              PairingProtocolFrame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_PROTOCOL_H */
