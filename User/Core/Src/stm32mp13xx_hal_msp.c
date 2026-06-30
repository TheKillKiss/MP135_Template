/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32mp13xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{

  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  /* System interrupt init*/

  /* Peripheral interrupt init */
  /* RCC_WAKEUP_IRQn interrupt configuration */
  IRQ_SetPriority(RCC_WAKEUP_IRQn, 0);
  IRQ_Enable(RCC_WAKEUP_IRQn);

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

/* USER CODE BEGIN 1 */

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
#if defined(CORE_CA7)
  if (TickPriority >= (1UL << 4)) {
    return HAL_ERROR;
  }

  uwTickPrio = TickPriority;
#else
  UNUSED(TickPriority);
#endif

  /*
   * The A7 generic timer is configured in SystemInit() and drives both
   * HAL_IncTick() and OSTimeTick() from SecurePhysicalTimer_IRQHandler().
   */
  return HAL_OK;
}

/* USER CODE END 1 */
