#ifndef BSP_EMMC_H__
#define BSP_EMMC_H__

#include "main.h"

#define BSP_EMMC_PART_USER              0U
#define BSP_EMMC_PART_BOOT1             1U
#define BSP_EMMC_PART_BOOT2             2U
#define BSP_EMMC_PART_RPMB              3U

extern MMC_HandleTypeDef hmmc2;

void BSP_EMMC_Init(void);
HAL_StatusTypeDef BSP_EMMC_GetCardInfo(HAL_MMC_CardInfoTypeDef *card_info);
HAL_StatusTypeDef BSP_EMMC_ReadBlocks(uint32_t lba, uint8_t *buf, uint32_t block_count);
HAL_StatusTypeDef BSP_EMMC_WriteBlocks(uint32_t lba, const uint8_t *buf, uint32_t block_count);
HAL_StatusTypeDef BSP_EMMC_EraseWholeChip(void);
HAL_StatusTypeDef BSP_EMMC_SwitchPart(uint8_t part);
HAL_StatusTypeDef BSP_EMMC_EnableBootPart(uint8_t boot_part);
HAL_StatusTypeDef BSP_EMMC_Read(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef BSP_EMMC_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

#endif
