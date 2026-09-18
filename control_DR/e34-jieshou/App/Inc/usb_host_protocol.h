#ifndef USB_HOST_PROTOCOL_H
#define USB_HOST_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Receiver -> host CDC stream. Distinct from the on-air A5 5A control frame. */
#define USB_HOST_PROTOCOL_SOF0            0xB5U
#define USB_HOST_PROTOCOL_SOF1            0x5BU
#define USB_HOST_PROTOCOL_VERSION         0x01U
#define USB_HOST_PROTOCOL_TYPE_CONTROL    0x01U
#define USB_HOST_PROTOCOL_PAYLOAD_SIZE    16U
#define USB_HOST_PROTOCOL_HEADER_SIZE     6U
#define USB_HOST_PROTOCOL_CRC_SIZE        2U
#define USB_HOST_PROTOCOL_FRAME_SIZE      (USB_HOST_PROTOCOL_HEADER_SIZE + \
                                           USB_HOST_PROTOCOL_PAYLOAD_SIZE + \
                                           USB_HOST_PROTOCOL_CRC_SIZE)
#define USB_HOST_PROTOCOL_PERIOD_MS       20U

/* Fill a 24-byte CONTROL frame from the current debug snapshot.
 * CRC-16/MODBUS covers VER..payload. Returns FRAME_SIZE. */
uint8_t UsbHostProtocol_BuildControl(uint8_t *out, uint8_t seq);

#ifdef __cplusplus
}
#endif

#endif /* USB_HOST_PROTOCOL_H */
