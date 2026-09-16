/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usb_device.c
  * @brief          : USB Device CDC init. SYSCLK stays on 16 MHz HSI; USB uses HSI48+CRS.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "debug.h"

USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_DescriptorsTypeDef CDC_Desc;

#define USB_INIT_ERR_NONE       0U
#define USB_INIT_ERR_CLOCK      1U
#define USB_INIT_ERR_CORE       2U
#define USB_INIT_ERR_CLASS      3U
#define USB_INIT_ERR_INTERFACE  4U
#define USB_INIT_ERR_START      5U

static uint8_t USB_Clock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /* Keep SYSCLK on HSI. Only turn on the independent 48 MHz USB clock. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    return 0U;
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    return 0U;
  }

  __HAL_RCC_CRS_CLK_ENABLE();
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000U, 1000U);
  RCC_CRSInitStruct.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
  RCC_CRSInitStruct.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
  return 1U;
}

void USBD_Clock_Config(void)
{
  (void)USB_Clock_Config();
}

void MX_USB_DEVICE_Init(void)
{
  debug.usb.init_ok = 0U;
  debug.usb.configured = 0U;

  if (USB_Clock_Config() == 0U)
  {
    debug.usb.init_error = USB_INIT_ERR_CLOCK;
    return;
  }

  if (USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS) != USBD_OK)
  {
    debug.usb.init_error = USB_INIT_ERR_CORE;
    return;
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
  {
    debug.usb.init_error = USB_INIT_ERR_CLASS;
    return;
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
  {
    debug.usb.init_error = USB_INIT_ERR_INTERFACE;
    return;
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
  {
    debug.usb.init_error = USB_INIT_ERR_START;
    return;
  }

  debug.usb.init_error = USB_INIT_ERR_NONE;
  debug.usb.init_ok = 1U;
}
