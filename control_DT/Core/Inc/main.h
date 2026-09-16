/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
/* 双轴摇杆模拟输入：0 至 3.3 V，对应 ADC 12 位原始值 0 至 4095。 */
#define JOY_LEFT_X_Pin GPIO_PIN_0
#define JOY_LEFT_X_GPIO_Port GPIOA
#define JOY_LEFT_Y_Pin GPIO_PIN_1
#define JOY_LEFT_Y_GPIO_Port GPIOA
#define JOY_RIGHT_X_Pin GPIO_PIN_2
#define JOY_RIGHT_X_GPIO_Port GPIOA
#define JOY_RIGHT_Y_Pin GPIO_PIN_4
#define JOY_RIGHT_Y_GPIO_Port GPIOA

/* 四路两档拨杆：A/B 触点均为低有效；高/高或低/低均为无效状态。 */
#define SWITCH_A_1_Pin GPIO_PIN_12
#define SWITCH_A_1_GPIO_Port GPIOB
#define SWITCH_B_1_Pin GPIO_PIN_13
#define SWITCH_B_1_GPIO_Port GPIOB

#define SWITCH_A_2_Pin GPIO_PIN_15
#define SWITCH_A_2_GPIO_Port GPIOA
#define SWITCH_B_2_Pin GPIO_PIN_9
#define SWITCH_B_2_GPIO_Port GPIOB

#define SWITCH_A_3_Pin GPIO_PIN_14
#define SWITCH_A_3_GPIO_Port GPIOC
#define SWITCH_B_3_Pin GPIO_PIN_13
#define SWITCH_B_3_GPIO_Port GPIOC

#define SWITCH_A_4_Pin GPIO_PIN_12
#define SWITCH_A_4_GPIO_Port GPIOA
#define SWITCH_B_4_Pin GPIO_PIN_11
#define SWITCH_B_4_GPIO_Port GPIOA

/* 四个按键：低电平表示按下，内部上拉使默认状态为高电平。 */
#define SW1_Pin GPIO_PIN_3
#define SW1_GPIO_Port GPIOA
#define SW2_Pin GPIO_PIN_5
#define SW2_GPIO_Port GPIOA
#define SW3_Pin GPIO_PIN_6
#define SW3_GPIO_Port GPIOA
#define SW4_Pin GPIO_PIN_7
#define SW4_GPIO_Port GPIOA

/* E22-400 / USART1：MCU TX=PA9 RX=PA10。配置模式 M0=0、M1=1。 */
#define AUX_Pin GPIO_PIN_8
#define AUX_GPIO_Port GPIOA
#define M0_Pin GPIO_PIN_14
#define M0_GPIO_Port GPIOB
#define M1_Pin GPIO_PIN_15
#define M1_GPIO_Port GPIOB

/* E22-900 / USART2：MCU TX=PB4 RX=PB3。 */
#define AUX_2_Pin GPIO_PIN_5
#define AUX_2_GPIO_Port GPIOB
#define M1_2_Pin GPIO_PIN_6
#define M1_2_GPIO_Port GPIOB
#define M0_2_Pin GPIO_PIN_7
#define M0_2_GPIO_Port GPIOB

/* E22-230 / USART3：MCU TX=PB10 RX=PB11。 */
#define AUX_3_Pin GPIO_PIN_2
#define AUX_3_GPIO_Port GPIOB
#define M0_3_Pin GPIO_PIN_0
#define M0_3_GPIO_Port GPIOB
#define M1_3_Pin GPIO_PIN_1
#define M1_3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
