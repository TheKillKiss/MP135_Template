/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    eth.c
  * @brief   This file provides code for the configuration
  *          of the ETH instances.
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
#include "eth.h"

#if defined ( __ICCARM__ ) /*!< IAR Compiler */

//#pragma location=0x30000000
__attribute__((aligned(32)))
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */

//#pragma location=0x30000100
__attribute__((aligned(32)))
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x30000000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x30000080))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __GNUC__ ) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")));   /* Ethernet Tx DMA Descriptors */

#endif
ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT * 2U];
ETH_TxPacketConfigTypeDef TxConfig;

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

ETH_HandleTypeDef heth1;

/* ETH1 init function */
void MX_ETH1_Init(void)
{

  /* USER CODE BEGIN ETH1_Init 0 */

  /* USER CODE END ETH1_Init 0 */

   static uint8_t MACAddr[6];

  /* USER CODE BEGIN ETH1_Init 1 */
  RCC->MP_AHB6ENSETR =
      RCC_MP_AHB6ENSETR_ETH1MACEN
    | RCC_MP_AHB6ENSETR_ETH1RXEN
    | RCC_MP_AHB6ENSETR_ETH1TXEN
    | RCC_MP_AHB6ENSETR_ETH1CKEN;
  /* USER CODE END ETH1_Init 1 */
  heth1.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth1.Init.MACAddr = &MACAddr[0];
  heth1.Init.MediaInterface = HAL_ETH_RGMII_MODE;
  heth1.Init.TxDesc = DMATxDscrTab;
  heth1.Init.RxDesc = DMARxDscrTab;
  heth1.Init.RxBuffLen = 1524;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth1) != HAL_OK)
  {
    Error_Handler();
  }

  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfigTypeDef));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  /* USER CODE BEGIN ETH1_Init 2 */

  /* USER CODE END ETH1_Init 2 */

}

void HAL_ETH_MspInit(ETH_HandleTypeDef* ethHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if(ethHandle->Instance==ETH)
  {
  /* USER CODE BEGIN ETH_MspInit 0 */

  /* USER CODE END ETH_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ETH1;
    PeriphClkInit.Eth1ClockSelection = RCC_ETH1CLKSOURCE_PLL4;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ETH clock enable */
    __HAL_RCC_ETH1CK_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    /**ETH1 GPIO Configuration
    PB11     ------> ETH1_TX_CTL
    PA2     ------> ETH1_MDIO
    PG13     ------> ETH1_TXD0
    PG14     ------> ETH1_TXD1
    PG2     ------> ETH1_MDC
    PA1     ------> ETH1_RX_CLK
    PC5     ------> ETH1_RXD1
    PF12     ------> ETH1_CLK125
    PB1     ------> ETH1_RXD3
    PE5     ------> ETH1_TXD3
    PA7     ------> ETH1_RX_CTL
    PC4     ------> ETH1_RXD0
    PB0     ------> ETH1_RXD2
    PC1     ------> ETH1_GTX_CLK
    PC2     ------> ETH1_TXD2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_ETH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN ETH_MspInit 1 */
    IRQ_SetPriority(ETH1_IRQn, 5);
    IRQ_SetMode(ETH1_IRQn,
                IRQ_MODE_TRIG_LEVEL | IRQ_MODE_TYPE_IRQ | IRQ_MODE_CPU_0);
    IRQ_Enable(ETH1_IRQn);

  /* USER CODE END ETH_MspInit 1 */
  }
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef* ethHandle)
{

  if(ethHandle->Instance==ETH)
  {
  /* USER CODE BEGIN ETH_MspDeInit 0 */

  /* USER CODE END ETH_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ETH1CK_CLK_DISABLE();

    /**ETH1 GPIO Configuration
    PB11     ------> ETH1_TX_CTL
    PA2     ------> ETH1_MDIO
    PG13     ------> ETH1_TXD0
    PG14     ------> ETH1_TXD1
    PG2     ------> ETH1_MDC
    PA1     ------> ETH1_RX_CLK
    PC5     ------> ETH1_RXD1
    PF12     ------> ETH1_CLK125
    PB1     ------> ETH1_RXD3
    PE5     ------> ETH1_TXD3
    PA7     ------> ETH1_RX_CTL
    PC4     ------> ETH1_RXD0
    PB0     ------> ETH1_RXD2
    PC1     ------> ETH1_GTX_CLK
    PC2     ------> ETH1_TXD2
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11|GPIO_PIN_1|GPIO_PIN_0);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_1|GPIO_PIN_7);

    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_2);

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5|GPIO_PIN_4|GPIO_PIN_1|GPIO_PIN_2);

    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_12);

    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_5);

  /* USER CODE BEGIN ETH_MspDeInit 1 */

  /* USER CODE END ETH_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

