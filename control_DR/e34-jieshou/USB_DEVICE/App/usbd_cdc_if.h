#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"

#define APP_RX_DATA_SIZE  256U
#define APP_TX_DATA_SIZE  512U

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);
uint16_t CDC_Read_FS(uint8_t *Buf, uint16_t MaxLen);
uint8_t CDC_IsConfigured(void);
uint8_t CDC_IsHostOpen(void);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
