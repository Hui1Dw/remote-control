/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @brief          : Non-blocking USB CDC VCP. RX is copied to a ring; TX never waits.
  ******************************************************************************
  */

#include "usbd_cdc_if.h"

#include <string.h>
#include "debug.h"
#include "usbd_def.h"
#include "stm32g4xx_hal.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

#define CDC_RX_RING_SIZE  256U

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS};

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];
static uint8_t rx_ring[CDC_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static uint8_t line_coding[7] = {0x00U, 0xC2U, 0x01U, 0x00U, 0x00U, 0x00U, 0x08U};
static volatile uint8_t cdc_dtr;
static uint32_t cdc_tx_started_ms;

static void CdcClearTxState(void)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (hcdc != NULL)
  {
    hcdc->TxState = 0U;
  }
}

static void CdcRingWrite(const uint8_t *data, uint32_t len)
{
  uint32_t i;

  for (i = 0U; i < len; i++)
  {
    uint16_t next = (uint16_t)((rx_head + 1U) % CDC_RX_RING_SIZE);
    if (next == rx_tail)
    {
      break;
    }
    rx_ring[rx_head] = data[i];
    rx_head = next;
  }
}

static int8_t CDC_Init_FS(void)
{
  rx_head = 0U;
  rx_tail = 0U;
  cdc_dtr = 0U;
  cdc_tx_started_ms = 0U;
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (int8_t)USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
  cdc_dtr = 0U;
  return (int8_t)USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
  switch (cmd)
  {
    case CDC_SET_LINE_CODING:
      if ((pbuf != NULL) && (length >= 7U))
      {
        (void)memcpy(line_coding, pbuf, 7U);
      }
      break;

    case CDC_GET_LINE_CODING:
      if ((pbuf != NULL) && (length >= 7U))
      {
        (void)memcpy(pbuf, line_coding, 7U);
      }
      break;

    case CDC_SET_CONTROL_LINE_STATE:
      /* ST stack passes the setup packet pointer when wLength=0. DTR=wValue bit0. */
      if ((pbuf != NULL) && (length == 0U))
      {
        const USBD_SetupReqTypedef *req = (const USBD_SetupReqTypedef *)pbuf;
        uint8_t dtr = ((req->wValue & 0x0001U) != 0U) ? 1U : 0U;
        if ((dtr != 0U) && (cdc_dtr == 0U))
        {
          CdcClearTxState();
        }
        cdc_dtr = dtr;
      }
      break;

    default:
      break;
  }

  return (int8_t)USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len)
{
  uint32_t n = (Len != NULL) ? *Len : 0U;

  if ((pbuf != NULL) && (n > 0U))
  {
    CdcRingWrite(pbuf, n);
    debug.usb.rx_count += n;
  }

  /* Re-arm immediately. Never wait for the host or the maintenance parser. */
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (int8_t)USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
  USBD_CDC_HandleTypeDef *hcdc;

  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (hcdc == NULL)
  {
    return (uint8_t)USBD_BUSY;
  }
  if (hcdc->TxState != 0U)
  {
    uint32_t now = HAL_GetTick();
    if ((now - cdc_tx_started_ms) < 100U)
    {
      return (uint8_t)USBD_BUSY;
    }
    /* Host was not reading; the IN packet never completed. Drop it and retry. */
    hcdc->TxState = 0U;
  }

  if ((Buf == NULL) || (Len == 0U))
  {
    return (uint8_t)USBD_OK;
  }

  if (Len > APP_TX_DATA_SIZE)
  {
    Len = (uint16_t)APP_TX_DATA_SIZE;
  }

  (void)memcpy(UserTxBufferFS, Buf, Len);
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, Len);
  cdc_tx_started_ms = HAL_GetTick();
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum)
{
  UNUSED(pbuf);
  UNUSED(Len);
  UNUSED(epnum);
  return (int8_t)USBD_OK;
}

uint16_t CDC_Read_FS(uint8_t *Buf, uint16_t MaxLen)
{
  uint16_t n = 0U;

  if ((Buf == NULL) || (MaxLen == 0U))
  {
    return 0U;
  }

  while ((n < MaxLen) && (rx_tail != rx_head))
  {
    Buf[n] = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % CDC_RX_RING_SIZE);
    n++;
  }

  return n;
}

uint8_t CDC_IsConfigured(void)
{
  return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
}

uint8_t CDC_IsHostOpen(void)
{
  return ((CDC_IsConfigured() != 0U) && (cdc_dtr != 0U)) ? 1U : 0U;
}
