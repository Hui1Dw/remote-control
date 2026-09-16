/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usb_device.h
  * @brief          : Header for usb_device.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __USB_DEVICE__H__
#define __USB_DEVICE__H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include "usbd_def.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

/** USB Device initialization. Failure must not stop the control path. */
void MX_USB_DEVICE_Init(void);

/** Resume helper used by usbd_conf.c. Does not change SYSCLK. */
void USBD_Clock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE__H__ */
