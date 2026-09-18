#include "usb_host_protocol.h"

#include "control_protocol.h"
#include "debug.h"

uint8_t UsbHostProtocol_BuildControl(uint8_t *out, uint8_t seq)
{
  uint16_t crc;
  uint16_t valid_frame_count = 0U;
  uint16_t apply_interval_ms;

  if (out == 0)
  {
    return 0U;
  }

  if (debug.active_source <= 2U)
  {
    valid_frame_count = (uint16_t)debug.link[debug.active_source].valid_frame_count;
  }

  apply_interval_ms = (debug.timing.last_apply_interval_ms > 0xFFFFU)
                          ? 0xFFFFU
                          : (uint16_t)debug.timing.last_apply_interval_ms;

  out[0] = USB_HOST_PROTOCOL_SOF0;
  out[1] = USB_HOST_PROTOCOL_SOF1;
  out[2] = USB_HOST_PROTOCOL_VERSION;
  out[3] = USB_HOST_PROTOCOL_TYPE_CONTROL;
  out[4] = seq;
  out[5] = USB_HOST_PROTOCOL_PAYLOAD_SIZE;
  out[6] = (uint8_t)debug.output.left_y;
  out[7] = (uint8_t)debug.output.left_x;
  out[8] = (uint8_t)debug.output.right_y;
  out[9] = (uint8_t)debug.output.right_x;
  out[10] = debug.output.switches;
  out[11] = debug.output.buttons;
  out[12] = debug.safe;
  out[13] = debug.safe_reason;
  out[14] = debug.active_source;
  out[15] = debug.output.sequence;
  out[16] = (uint8_t)(valid_frame_count & 0xFFU);
  out[17] = (uint8_t)(valid_frame_count >> 8U);
  out[18] = (uint8_t)(apply_interval_ms & 0xFFU);
  out[19] = (uint8_t)(apply_interval_ms >> 8U);
  out[20] = 0U;
  out[21] = 0U;

  crc = ControlProtocol_Crc16Modbus(&out[2],
                                    (uint16_t)(USB_HOST_PROTOCOL_HEADER_SIZE - 2U +
                                               USB_HOST_PROTOCOL_PAYLOAD_SIZE));
  out[22] = (uint8_t)(crc & 0xFFU);
  out[23] = (uint8_t)(crc >> 8U);
  return USB_HOST_PROTOCOL_FRAME_SIZE;
}
