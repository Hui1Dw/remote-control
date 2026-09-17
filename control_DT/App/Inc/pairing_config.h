#ifndef PAIRING_CONFIG_H
#define PAIRING_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Factory/default fixed-transfer identities.  They are used until a completed
 * field-pairing record is stored in the MCU flash. */
#define PAIRING_CONFIG_DEFAULT_TX_ADDRESS  ((uint16_t)0x0107U) /* Debug pair 7 */
#define PAIRING_CONFIG_DEFAULT_RX_ADDRESS  ((uint16_t)0x0207U) /* Debug pair 7 */
#define PAIRING_CONFIG_MIN_PAIR_NUMBER     ((uint8_t)1U)
#define PAIRING_CONFIG_MAX_PAIR_NUMBER     ((uint8_t)63U)

/* Logical pair number N maps to distinct transmitter/receiver address ranges:
 * TX = 0x0100 + N, RX = 0x0200 + N. */
#define PAIRING_CONFIG_TX_ADDRESS_BASE     ((uint16_t)0x0100U)
#define PAIRING_CONFIG_RX_ADDRESS_BASE     ((uint16_t)0x0200U)

typedef struct
{
  uint16_t transmitter_address;
  uint16_t receiver_address;
  uint8_t pair_number;
  uint8_t factory_default;
  uint32_t generation;
} PairingConfig_t;

/* Build a deterministic address pair for a human-entered pair number. */
uint8_t PairingConfig_Derive(uint8_t pair_number,
                             uint16_t *transmitter_address,
                             uint16_t *receiver_address);

/* Fill config with the common factory/default fixed-transfer addresses. */
void PairingConfig_SetFactoryDefault(PairingConfig_t *config);

/* Load the latest CRC-valid configuration from the reserved two-slot MCU
 * flash area.  If neither slot is valid, factory defaults are returned and
 * the function returns 0. */
uint8_t PairingConfig_Load(PairingConfig_t *config);

/* Persist a completed pairing configuration by writing the inactive flash
 * slot first.  Boot code must not call this; only a completed pairing
 * transaction writes flash. */
uint8_t PairingConfig_Save(const PairingConfig_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_CONFIG_H */
