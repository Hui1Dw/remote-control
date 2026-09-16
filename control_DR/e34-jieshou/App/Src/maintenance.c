#include "maintenance.h"

#include "board.h"
#include "debug.h"

void Maintenance_Poll(void)
{
  uint8_t chunk[32];
  uint16_t n;

  Board_UsbUpdateDebug();
  if ((debug.usb.init_ok == 0U) || (debug.usb.configured == 0U))
  {
    return;
  }

  /* Loopback only. No command protocol in this stage. */
  n = Board_UsbRead(chunk, (uint16_t)sizeof(chunk));
  if (n == 0U)
  {
    return;
  }

  if (Board_UsbWrite(chunk, n) != 0U)
  {
    debug.usb.cmd_count++;
  }
}
