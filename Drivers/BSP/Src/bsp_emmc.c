#include "bsp_emmc.h"
#include "irq_ctrl.h"
#include <string.h>

#define BSP_EMMC_EXT_CSD_PART_CONFIG    179U
#define BSP_EMMC_TIMEOUT_MS             5000U
#define BSP_EMMC_IRQ_PRIORITY           0xA0U

MMC_HandleTypeDef hmmc2;
static volatile uint8_t emmc_tx_done = 0U;
static volatile uint8_t emmc_rx_done = 0U;
static uint8_t emmc_ext_csd[MMC_BLOCKSIZE] __attribute__((aligned(32)));

static void bsp_emmc_irq_handler(void)
{
    HAL_MMC_IRQHandler(&hmmc2);
}

static HAL_StatusTypeDef bsp_emmc_wait_transfer(void)
{
    uint32_t start = HAL_GetTick();

    while (HAL_MMC_GetCardState(&hmmc2) != HAL_MMC_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - start) >= BSP_EMMC_TIMEOUT_MS)
        {
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

void BSP_EMMC_Init(void)
{
    __HAL_RCC_SDMMC2_FORCE_RESET();
    __HAL_RCC_SDMMC2_RELEASE_RESET();

    hmmc2.Instance = SDMMC2;
    hmmc2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hmmc2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hmmc2.Init.BusWide = SDMMC_BUS_WIDE_8B;
    hmmc2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hmmc2.Init.ClockDiv = 250U;

    if (HAL_MMC_Init(&hmmc2) != HAL_OK)
    {
        Error_Handler();
    }

    SDMMC2->CLKCR &= ~SDMMC_CLKCR_CLKDIV;
    SDMMC2->CLKCR |= 5U;
}

void HAL_MMC_MspInit(MMC_HandleTypeDef *mmcHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    if (mmcHandle->Instance == SDMMC2)
    {
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC2;
        PeriphClkInit.Sdmmc2ClockSelection = RCC_SDMMC2CLKSOURCE_PLL3;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_RCC_SDMMC2_CLK_ENABLE();

        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        __HAL_RCC_GPIOF_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /*
         * SDMMC2 eMMC:
         * PC7 D7, PB9 D5, PB14 D0, PG6 CMD, PC6 D6,
         * PB15 D1, PB4 D3, PB3 D2, PF0 D4, PE3 CK.
         */
        GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_SDIO2;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_4 | GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_SDIO2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_SDIO2;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_SDIO2;
        HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_SDIO2;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        IRQ_Disable(SDMMC2_IRQn);
        IRQ_ClearPending(SDMMC2_IRQn);
        IRQ_SetHandler(SDMMC2_IRQn, bsp_emmc_irq_handler);
        IRQ_SetPriority(SDMMC2_IRQn, BSP_EMMC_IRQ_PRIORITY);
        IRQ_SetMode(SDMMC2_IRQn, IRQ_MODE_TRIG_LEVEL);
        IRQ_Enable(SDMMC2_IRQn);
    }
}

void HAL_MMC_MspDeInit(MMC_HandleTypeDef *mmcHandle)
{
    if (mmcHandle->Instance == SDMMC2)
    {
        __HAL_RCC_SDMMC2_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_7 | GPIO_PIN_6);
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_9 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_4 | GPIO_PIN_3);
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);
        HAL_GPIO_DeInit(GPIOF, GPIO_PIN_0);
        HAL_GPIO_DeInit(GPIOE, GPIO_PIN_3);
    }
}

void HAL_MMC_TxCpltCallback(MMC_HandleTypeDef *hmmc)
{
    if (hmmc == &hmmc2)
    {
        emmc_tx_done = 1U;
    }
}

void HAL_MMC_RxCpltCallback(MMC_HandleTypeDef *hmmc)
{
    if (hmmc == &hmmc2)
    {
        emmc_rx_done = 1U;
    }
}

HAL_StatusTypeDef BSP_EMMC_GetCardInfo(HAL_MMC_CardInfoTypeDef *card_info)
{
    if (card_info == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_MMC_GetCardInfo(&hmmc2, card_info);
}

HAL_StatusTypeDef BSP_EMMC_ReadBlocks(uint32_t lba, uint8_t *buf, uint32_t block_count)
{
    if ((buf == NULL) || (block_count == 0U))
    {
        return HAL_ERROR;
    }

    if (bsp_emmc_wait_transfer() != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    emmc_rx_done = 0U;
    if (HAL_MMC_ReadBlocks_DMA(&hmmc2, buf, lba, block_count) != HAL_OK)
    {
        return HAL_ERROR;
    }

    while (!emmc_rx_done)
    {
    }

    return bsp_emmc_wait_transfer();
}

HAL_StatusTypeDef BSP_EMMC_WriteBlocks(uint32_t lba, const uint8_t *buf, uint32_t block_count)
{
    if ((buf == NULL) || (block_count == 0U))
    {
        return HAL_ERROR;
    }

    if (bsp_emmc_wait_transfer() != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    emmc_tx_done = 0U;
    if (HAL_MMC_WriteBlocks_DMA(&hmmc2, (uint8_t *)buf, lba, block_count) != HAL_OK)
    {
        return HAL_ERROR;
    }

    while (!emmc_tx_done)
    {
    }

    return bsp_emmc_wait_transfer();
}

HAL_StatusTypeDef BSP_EMMC_EraseWholeChip(void)
{
    HAL_MMC_CardInfoTypeDef card_info;
    HAL_StatusTypeDef status;

    status = HAL_MMC_GetCardInfo(&hmmc2, &card_info);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_MMC_Erase(&hmmc2, 0U, card_info.BlockNbr - 1U);
}

HAL_StatusTypeDef BSP_EMMC_SwitchPart(uint8_t part)
{
    uint32_t arg;
    uint8_t part_cfg;

    if (bsp_emmc_wait_transfer() != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    HAL_MMC_ConfigWideBusOperation(&hmmc2, SDMMC_BUS_WIDE_1B);

    if (HAL_MMC_GetCardExtCSD(&hmmc2, (uint32_t *)emmc_ext_csd, BSP_EMMC_TIMEOUT_MS) != HAL_OK)
    {
        (void)HAL_MMC_ConfigWideBusOperation(&hmmc2, SDMMC_BUS_WIDE_8B);
        return HAL_ERROR;
    }

    HAL_MMC_ConfigWideBusOperation(&hmmc2, SDMMC_BUS_WIDE_8B);

    part_cfg = emmc_ext_csd[BSP_EMMC_EXT_CSD_PART_CONFIG];
    part_cfg = (part_cfg & 0xF8U) | (part & 0x07U);

    SDMMC2->DCTRL = 0U;
    SDMMC2->ICR = 0xFFFFFFFFU;

    arg = (3UL << 24) | ((uint32_t)BSP_EMMC_EXT_CSD_PART_CONFIG << 16) | ((uint32_t)part_cfg << 8);

    if (SDMMC_CmdSwitch(SDMMC2, arg) != SDMMC_ERROR_NONE)
    {
        return HAL_ERROR;
    }

    while (__SDMMC_GET_FLAG(SDMMC2, SDMMC_FLAG_BUSYD0))
    {
    }

    return bsp_emmc_wait_transfer();
}

HAL_StatusTypeDef BSP_EMMC_EnableBootPart(uint8_t boot_part)
{
    uint32_t arg;
    uint8_t value;

    if (bsp_emmc_wait_transfer() != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    SDMMC2->DCTRL = 0U;
    SDMMC2->ICR = 0xFFFFFFFFU;

    if (boot_part == BSP_EMMC_PART_BOOT1)
    {
        value = 0x48U;
    }
    else if (boot_part == BSP_EMMC_PART_BOOT2)
    {
        value = 0x50U;
    }
    else
    {
        return HAL_ERROR;
    }

    arg = (3UL << 24) | ((uint32_t)BSP_EMMC_EXT_CSD_PART_CONFIG << 16) | ((uint32_t)value << 8);
    if (SDMMC_CmdSwitch(SDMMC2, arg) != SDMMC_ERROR_NONE)
    {
        return HAL_ERROR;
    }

    while (__SDMMC_GET_FLAG(SDMMC2, SDMMC_FLAG_BUSYD0))
    {
    }

    return bsp_emmc_wait_transfer();
}

HAL_StatusTypeDef BSP_EMMC_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t lba = addr / MMC_BLOCKSIZE;
    uint32_t offset = addr % MMC_BLOCKSIZE;

    if ((buf == NULL) && (len > 0U))
    {
        return HAL_ERROR;
    }

    if (offset)
    {
        static uint8_t block_buf[MMC_BLOCKSIZE] __attribute__((aligned(32)));
        uint32_t copy = MMC_BLOCKSIZE - offset;

        if (copy > len)
        {
            copy = len;
        }

        if (BSP_EMMC_ReadBlocks(lba, block_buf, 1U) != HAL_OK)
        {
            return HAL_ERROR;
        }

        memcpy(&block_buf[offset], buf, copy);

        if (BSP_EMMC_WriteBlocks(lba, block_buf, 1U) != HAL_OK)
        {
            return HAL_ERROR;
        }

        addr += copy;
        buf += copy;
        len -= copy;
        lba++;
    }

    if (len / MMC_BLOCKSIZE)
    {
        uint32_t full_blocks = len / MMC_BLOCKSIZE;
        uint32_t bytes = full_blocks * MMC_BLOCKSIZE;

        if (BSP_EMMC_WriteBlocks(lba, buf, full_blocks) != HAL_OK)
        {
            return HAL_ERROR;
        }

        addr += bytes;
        buf += bytes;
        len -= bytes;
        lba += full_blocks;
    }

    if (len)
    {
        static uint8_t block_buf[MMC_BLOCKSIZE] __attribute__((aligned(32)));

        if (BSP_EMMC_ReadBlocks(lba, block_buf, 1U) != HAL_OK)
        {
            return HAL_ERROR;
        }

        memcpy(block_buf, buf, len);

        if (BSP_EMMC_WriteBlocks(lba, block_buf, 1U) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef BSP_EMMC_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t lba = addr / MMC_BLOCKSIZE;
    uint32_t offset = addr % MMC_BLOCKSIZE;

    if ((buf == NULL) && (len > 0U))
    {
        return HAL_ERROR;
    }

    if (offset)
    {
        static uint8_t block_buf[MMC_BLOCKSIZE] __attribute__((aligned(32)));
        uint32_t copy = MMC_BLOCKSIZE - offset;

        if (copy > len)
        {
            copy = len;
        }

        if (BSP_EMMC_ReadBlocks(lba, block_buf, 1U) != HAL_OK)
        {
            return HAL_ERROR;
        }

        memcpy(buf, &block_buf[offset], copy);

        addr += copy;
        buf += copy;
        len -= copy;
        lba++;
    }

    if (len / MMC_BLOCKSIZE)
    {
        uint32_t full_blocks = len / MMC_BLOCKSIZE;
        uint32_t bytes = full_blocks * MMC_BLOCKSIZE;

        if (BSP_EMMC_ReadBlocks(lba, buf, full_blocks) != HAL_OK)
        {
            return HAL_ERROR;
        }

        addr += bytes;
        buf += bytes;
        len -= bytes;
        lba += full_blocks;
    }

    if (len)
    {
        static uint8_t block_buf[MMC_BLOCKSIZE] __attribute__((aligned(32)));

        if (BSP_EMMC_ReadBlocks(lba, block_buf, 1U) != HAL_OK)
        {
            return HAL_ERROR;
        }

        memcpy(buf, block_buf, len);
    }

    return HAL_OK;
}
