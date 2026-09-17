#include "pairing_config.h"

#include "board.h"

#include <stddef.h>
#include <string.h>

#define PAIRING_CONFIG_MAGIC               ((uint32_t)0x50414952U) /* "PAIR" */
#define PAIRING_CONFIG_VERSION             ((uint16_t)1U)
#define PAIRING_CONFIG_STATE_ACTIVE        ((uint8_t)0xA5U)

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t record_size;
  uint32_t generation;
  uint16_t transmitter_address;
  uint16_t receiver_address;
  uint8_t pair_number;
  uint8_t state;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t crc32;
} PairingConfigFlashRecord_t;

_Static_assert(sizeof(PairingConfigFlashRecord_t) == 32U,
               "Pairing flash record must stay doubleword aligned");
_Static_assert((sizeof(PairingConfigFlashRecord_t) % 8U) == 0U,
               "Pairing flash record must be programmed in doublewords");

static uint32_t PairingConfig_Crc32(const uint8_t *data, uint32_t length)
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

uint8_t PairingConfig_Derive(uint8_t pair_number,
                             uint16_t *transmitter_address,
                             uint16_t *receiver_address)
{
  if (pair_number < PAIRING_CONFIG_MIN_PAIR_NUMBER ||
      pair_number > PAIRING_CONFIG_MAX_PAIR_NUMBER ||
      transmitter_address == NULL || receiver_address == NULL)
  {
    return 0U;
  }

  *transmitter_address = (uint16_t)(PAIRING_CONFIG_TX_ADDRESS_BASE + pair_number);
  *receiver_address = (uint16_t)(PAIRING_CONFIG_RX_ADDRESS_BASE + pair_number);
  return 1U;
}

void PairingConfig_SetFactoryDefault(PairingConfig_t *config)
{
  if (config == NULL)
  {
    return;
  }

  config->transmitter_address = PAIRING_CONFIG_DEFAULT_TX_ADDRESS;
  config->receiver_address = PAIRING_CONFIG_DEFAULT_RX_ADDRESS;
  config->pair_number = 0U;
  config->factory_default = 1U;
  config->generation = 0U;
}

static uint8_t PairingConfig_RecordIsValid(const PairingConfigFlashRecord_t *record)
{
  uint16_t expected_tx_address;
  uint16_t expected_rx_address;

  if (record == NULL ||
      record->magic != PAIRING_CONFIG_MAGIC ||
      record->version != PAIRING_CONFIG_VERSION ||
      record->record_size != sizeof(PairingConfigFlashRecord_t) ||
      record->state != PAIRING_CONFIG_STATE_ACTIVE ||
      PairingConfig_Derive(record->pair_number, &expected_tx_address,
                           &expected_rx_address) == 0U ||
      record->transmitter_address != expected_tx_address ||
      record->receiver_address != expected_rx_address ||
      record->crc32 != PairingConfig_Crc32((const uint8_t *)record,
          (uint32_t)offsetof(PairingConfigFlashRecord_t, crc32)))
  {
    return 0U;
  }

  return 1U;
}

static const PairingConfigFlashRecord_t *PairingConfig_RecordAt(uint8_t slot)
{
  const uint8_t *bytes = Board_PairingSlotBytes(slot);

  if (bytes == NULL)
  {
    return NULL;
  }

  return (const PairingConfigFlashRecord_t *)(const void *)bytes;
}

static uint8_t PairingConfig_GenerationIsNewer(uint32_t candidate,
                                                uint32_t reference)
{
  return ((int32_t)(candidate - reference) > 0) ? 1U : 0U;
}

static const PairingConfigFlashRecord_t *PairingConfig_LatestRecord(uint8_t *slot_out)
{
  const PairingConfigFlashRecord_t *slot0 = PairingConfig_RecordAt(0U);
  const PairingConfigFlashRecord_t *slot1 = PairingConfig_RecordAt(1U);
  uint8_t slot0_valid = PairingConfig_RecordIsValid(slot0);
  uint8_t slot1_valid = PairingConfig_RecordIsValid(slot1);

  if (slot0_valid == 0U)
  {
    if (slot1_valid != 0U)
    {
      if (slot_out != NULL)
      {
        *slot_out = 1U;
      }
      return slot1;
    }
    return NULL;
  }
  if (slot1_valid == 0U)
  {
    if (slot_out != NULL)
    {
      *slot_out = 0U;
    }
    return slot0;
  }

  if (PairingConfig_GenerationIsNewer(slot1->generation, slot0->generation) != 0U)
  {
    if (slot_out != NULL)
    {
      *slot_out = 1U;
    }
    return slot1;
  }

  if (slot_out != NULL)
  {
    *slot_out = 0U;
  }
  return slot0;
}

uint8_t PairingConfig_Load(PairingConfig_t *config)
{
  const PairingConfigFlashRecord_t *record;

  if (config == NULL)
  {
    return 0U;
  }

  record = PairingConfig_LatestRecord(NULL);
  if (record == NULL)
  {
    PairingConfig_SetFactoryDefault(config);
    return 0U;
  }

  config->transmitter_address = record->transmitter_address;
  config->receiver_address = record->receiver_address;
  config->pair_number = record->pair_number;
  config->factory_default = 0U;
  config->generation = record->generation;
  return 1U;
}

uint8_t PairingConfig_Save(const PairingConfig_t *config)
{
  PairingConfigFlashRecord_t record = {0};
  const PairingConfigFlashRecord_t *current;
  uint8_t current_slot = 0U;
  uint8_t target_slot;

  if (config == NULL ||
      PairingConfig_Derive(config->pair_number, &record.transmitter_address,
                           &record.receiver_address) == 0U ||
      config->transmitter_address != record.transmitter_address ||
      config->receiver_address != record.receiver_address)
  {
    return 0U;
  }

  current = PairingConfig_LatestRecord(&current_slot);
  target_slot = (current != NULL && current_slot == 0U) ? 1U : 0U;

  record.magic = PAIRING_CONFIG_MAGIC;
  record.version = PAIRING_CONFIG_VERSION;
  record.record_size = sizeof(record);
  record.generation = (current != NULL) ? current->generation + 1U : 1U;
  record.pair_number = config->pair_number;
  record.state = PAIRING_CONFIG_STATE_ACTIVE;
  record.crc32 = PairingConfig_Crc32((const uint8_t *)&record,
      (uint32_t)offsetof(PairingConfigFlashRecord_t, crc32));

  if (Board_PairingEraseSlot(target_slot) == 0U ||
      Board_PairingProgramSlot(target_slot, (const uint8_t *)&record,
                               sizeof(record)) == 0U ||
      PairingConfig_RecordIsValid(PairingConfig_RecordAt(target_slot)) == 0U)
  {
    return 0U;
  }

  return 1U;
}
