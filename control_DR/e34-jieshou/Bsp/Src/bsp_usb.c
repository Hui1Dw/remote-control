#include "bsp.h"

#include "debug.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"

uint8_t Bsp_UsbWrite(const uint8_t *data, uint16_t len)
{
  uint8_t status;

  if ((data == NULL) || (len == 0U))
  {
    return 1U;
  }

  status = CDC_Transmit_FS((uint8_t *)data, len);
  if (status == (uint8_t)USBD_OK)
  {
    debug.usb.tx_ok++;
    return 1U;
  }

  debug.usb.tx_busy_drop++;
  return 0U;
}

uint16_t Bsp_UsbRead(uint8_t *data, uint16_t max_len)
{
  return CDC_Read_FS(data, max_len);
}

uint8_t Bsp_UsbConfigured(void)
{
  return CDC_IsConfigured();
}
