#include "bsp_spi.h"

SPI_HandleTypeDef hspi5;

void BSP_SPI5_Init(void)
{
    hspi5.Instance = SPI5;
    hspi5.Init.Mode = SPI_MODE_MASTER;
    hspi5.Init.Direction = SPI_DIRECTION_2LINES;
    hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi5.Init.NSS = SPI_NSS_SOFT;
    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi5.Init.CRCPolynomial = 0x0U;
    hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi5.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi5.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi5.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi5.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi5.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi5.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi5.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi5.Init.IOSwap = SPI_IO_SWAP_DISABLE;

    if (HAL_SPI_Init(&hspi5) != HAL_OK)
    {
        Error_Handler();
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    if (spiHandle->Instance == SPI5)
    {
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SPI5;
        PeriphClkInit.Spi5ClockSelection = RCC_SPI5CLKSOURCE_PCLK6;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_RCC_SPI5_CLK_ENABLE();
        __HAL_RCC_GPIOI_CLK_ENABLE();
        __HAL_RCC_GPIOH_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /*
         * SPI5 to W25Q128:
         * PH12 MOSI, PH7 SCK, PE4 MISO, PI1 software NSS.
         */
        GPIO_InitStruct.Pin = GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
        HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
        GPIO_InitStruct.Alternate = GPIO_AF6_SPI5;
        HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
        GPIO_InitStruct.Alternate = GPIO_AF1_SPI5;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_SET);
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *spiHandle)
{
    if (spiHandle->Instance == SPI5)
    {
        __HAL_RCC_SPI5_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOH, GPIO_PIN_12 | GPIO_PIN_7);
        HAL_GPIO_DeInit(GPIOE, GPIO_PIN_4);
        HAL_GPIO_DeInit(GPIOI, GPIO_PIN_1);
    }
}
