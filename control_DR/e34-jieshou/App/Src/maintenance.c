#include "maintenance.h"

#include "board.h"
#include "debug.h"
#include "usb_host_protocol.h"

static uint8_t usb_stream_seq;
static uint8_t usb_stream_has_tick;
static uint32_t usb_stream_last_tick;

void Maintenance_Poll(void)
{
  uint8_t junk[32];
  uint8_t frame[USB_HOST_PROTOCOL_FRAME_SIZE];
  uint32_t now;

  Board_UsbUpdateDebug();
  if ((debug.usb.init_ok == 0U) || (debug.usb.configured == 0U))
  {
    usb_stream_has_tick = 0U;
    return;
  }

  /* v1 has no host commands. Drain so the CDC RX ring cannot fill. */
  while (Board_UsbRead(junk, (uint16_t)sizeof(junk)) != 0U)
  {
  }

  if (Board_UsbHostOpen() == 0U)
  {
    /* Do not queue an IN packet before the host opens the ACM port.
     * That leaves TxState stuck and later CLI reads nothing. */
    usb_stream_has_tick = 0U;
    return;
  }

  now = Board_NowMs();
  if ((usb_stream_has_tick != 0U) &&
      ((now - usb_stream_last_tick) < USB_HOST_PROTOCOL_PERIOD_MS))
  {
    return;
  }

  (void)UsbHostProtocol_BuildControl(frame, usb_stream_seq);
  if (Board_UsbWrite(frame, USB_HOST_PROTOCOL_FRAME_SIZE) != 0U)
  {
    usb_stream_seq++;
    usb_stream_last_tick = now;
    usb_stream_has_tick = 1U;
    debug.usb.cmd_count++;
  }
}
