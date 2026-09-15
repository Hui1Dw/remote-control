/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, M1_Pin|M0_Pin|M0_2_Pin|M1_2_Pin|M0_3_Pin|M1_3_Pin, GPIO_PIN_RESET);

  /* USART1 E34 模块：M0=PB15, M1=PB14 (刷为输出低 = 普通通信模式) */
  GPIO_InitStruct.Pin = M1_Pin|M0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USART1 E34 模块：AUX=PA8 (输入，监测模块状态) */
  GPIO_InitStruct.Pin = AUX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(AUX_GPIO_Port, &GPIO_InitStruct);

  /* 四路两档拨杆：A/B 触点低有效；高/高或低/低均视为无效。 */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;

  GPIO_InitStruct.Pin = SWITCH_A_1_Pin | SWITCH_B_1_Pin | SWITCH_B_2_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SWITCH_A_2_Pin | SWITCH_A_4_Pin | SWITCH_B_4_Pin;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* SW1-SW4 按键：低有效，内部上拉使释放状态为高电平。 */
  GPIO_InitStruct.Pin = SW1_Pin | SW2_Pin | SW3_Pin | SW4_Pin;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SWITCH_A_3_Pin | SWITCH_B_3_Pin;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USART2 E34 模块：M0=PB7, M1=PB6 (输出低 = 普通通信模式) */
  GPIO_InitStruct.Pin = M0_2_Pin|M1_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USART2 E34 模块：AUX=PB5 (输入) */
  GPIO_InitStruct.Pin = AUX_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(AUX_2_GPIO_Port, &GPIO_InitStruct);

  /* USART3 E34 模块：M0=PB1, M1=PB2 (输出低 = 普通通信模式) */
  GPIO_InitStruct.Pin = M0_3_Pin|M1_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USART3 E34 模块：AUX=PB0 (输入) */
  GPIO_InitStruct.Pin = AUX_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(AUX_3_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
