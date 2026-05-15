#ifndef BSP_W25Q128_H__
#define BSP_W25Q128_H__

#include "main.h"
#include <stdbool.h>

#define BSP_W25Q128_CMD_WRITE_ENABLE           0x06U
#define BSP_W25Q128_CMD_WRITE_DISABLE          0x04U
#define BSP_W25Q128_CMD_READ_STATUS1           0x05U
#define BSP_W25Q128_CMD_READ_STATUS2           0x35U
#define BSP_W25Q128_CMD_READ_STATUS3           0x15U
#define BSP_W25Q128_CMD_READ_DATA              0x03U
#define BSP_W25Q128_CMD_FAST_READ              0x0BU
#define BSP_W25Q128_CMD_PAGE_PROGRAM           0x02U
#define BSP_W25Q128_CMD_SECTOR_ERASE_4K        0x20U
#define BSP_W25Q128_CMD_BLOCK_ERASE_32K        0x52U
#define BSP_W25Q128_CMD_BLOCK_ERASE_64K        0xD8U
#define BSP_W25Q128_CMD_CHIP_ERASE_1           0xC7U
#define BSP_W25Q128_CMD_CHIP_ERASE_2           0x60U
#define BSP_W25Q128_CMD_POWER_DOWN             0xB9U
#define BSP_W25Q128_CMD_RELEASE_POWER_DOWN     0xABU
#define BSP_W25Q128_CMD_READ_UNIQUE_ID         0x4BU
#define BSP_W25Q128_CMD_JEDEC_ID               0x9FU
#define BSP_W25Q128_CMD_ENABLE_RESET           0x66U
#define BSP_W25Q128_CMD_RESET_DEVICE           0x99U

#define BSP_W25Q128_SR1_BUSY                   0x01U
#define BSP_W25Q128_SR1_WEL                    0x02U

#define BSP_W25Q128_FLASH_SIZE                 (16UL * 1024UL * 1024UL)
#define BSP_W25Q128_PAGE_SIZE                  256UL
#define BSP_W25Q128_SECTOR_SIZE                4096UL
#define BSP_W25Q128_BLOCK32_SIZE               (32UL * 1024UL)
#define BSP_W25Q128_BLOCK64_SIZE               (64UL * 1024UL)

#define BSP_W25Q128_PAGE_COUNT                 (BSP_W25Q128_FLASH_SIZE / BSP_W25Q128_PAGE_SIZE)
#define BSP_W25Q128_SECTOR_COUNT               (BSP_W25Q128_FLASH_SIZE / BSP_W25Q128_SECTOR_SIZE)
#define BSP_W25Q128_BLOCK64_COUNT              (BSP_W25Q128_FLASH_SIZE / BSP_W25Q128_BLOCK64_SIZE)

#define BSP_W25Q128_JEDEC_MID                  0xEFU
#define BSP_W25Q128_JEDEC_MEMTYPE_A            0x40U
#define BSP_W25Q128_JEDEC_MEMTYPE_B            0x70U
#define BSP_W25Q128_JEDEC_CAPACITY             0x18U

#define BSP_W25Q128_SPI_TIMEOUT_MS             1000U
#define BSP_W25Q128_BUSY_TIMEOUT_MS            5000U
#define BSP_W25Q128_CHIP_ERASE_TIMEOUT_MS      120000U

typedef struct
{
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
} bsp_w25q128_id_t;

typedef enum
{
    BSP_W25Q128_OK = 0,
    BSP_W25Q128_ERROR,
    BSP_W25Q128_TIMEOUT,
    BSP_W25Q128_PARAM_ERROR
} bsp_w25q128_status_t;

bsp_w25q128_status_t BSP_W25Q128_Init(void);
bsp_w25q128_status_t BSP_W25Q128_Reset(void);

bsp_w25q128_status_t BSP_W25Q128_ReadID(bsp_w25q128_id_t *id);
bool BSP_W25Q128_IsPresent(void);
bsp_w25q128_status_t BSP_W25Q128_ReadUniqueID(uint8_t uid[8]);

uint8_t BSP_W25Q128_ReadStatus1(void);
uint8_t BSP_W25Q128_ReadStatus2(void);
uint8_t BSP_W25Q128_ReadStatus3(void);
bool BSP_W25Q128_IsBusy(void);
bsp_w25q128_status_t BSP_W25Q128_WaitWhileBusy(uint32_t timeout_ms);

bsp_w25q128_status_t BSP_W25Q128_WriteEnable(void);
bsp_w25q128_status_t BSP_W25Q128_WriteDisable(void);

bsp_w25q128_status_t BSP_W25Q128_Read(uint32_t address, uint8_t *buffer, uint32_t length);
bsp_w25q128_status_t BSP_W25Q128_FastRead(uint32_t address, uint8_t *buffer, uint32_t length);
bsp_w25q128_status_t BSP_W25Q128_PageProgram(uint32_t address, const uint8_t *buffer, uint16_t length);
bsp_w25q128_status_t BSP_W25Q128_Write(uint32_t address, const uint8_t *buffer, uint32_t length);

bsp_w25q128_status_t BSP_W25Q128_EraseSector(uint32_t address);
bsp_w25q128_status_t BSP_W25Q128_Erase32Block(uint32_t address);
bsp_w25q128_status_t BSP_W25Q128_Erase64Block(uint32_t address);
bsp_w25q128_status_t BSP_W25Q128_ChipErase(void);

bsp_w25q128_status_t BSP_W25Q128_PowerDown(void);
bsp_w25q128_status_t BSP_W25Q128_ReleasePowerDown(void);

#endif
