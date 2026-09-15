/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USART1 E22-400：MCU TX=PA9 RX=PA10，模块 TXD→PA10、RXD←PA9。 */
#define AUX_1_Pin GPIO_PIN_8
#define AUX_1_GPIO_Port GPIOA
#define M1_1_Pin GPIO_PIN_15
#define M1_1_GPIO_Port GPIOB
#define M0_1_Pin GPIO_PIN_14
#define M0_1_GPIO_Port GPIOB

/* USART2 E22-900：MCU TX=PB3 RX=PB4，模块 TXD→PB4、RXD←PB3。 */
#define AUX_2_Pin GPIO_PIN_5
#define AUX_2_GPIO_Port GPIOB
#define M1_2_Pin GPIO_PIN_6
#define M1_2_GPIO_Port GPIOB
#define M0_2_Pin GPIO_PIN_7
#define M0_2_GPIO_Port GPIOB

/* USART3 E22-230：MCU TX=PB10 RX=PB11，模块 TXD→PB11、RXD←PB10。 */
#define AUX_3_Pin GPIO_PIN_2
#define AUX_3_GPIO_Port GPIOB
#define M1_3_Pin GPIO_PIN_1
#define M1_3_GPIO_Port GPIOB
#define M0_3_Pin GPIO_PIN_0
#define M0_3_GPIO_Port GPIOB

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
