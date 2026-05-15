#include "bsp_w25q128.h"
#include "bsp_spi.h"
#include <string.h>

#define BSP_W25Q128_CS_PORT             GPIOI
#define BSP_W25Q128_CS_PIN              GPIO_PIN_1

#define BSP_W25Q128_CS_LOW()            HAL_GPIO_WritePin(BSP_W25Q128_CS_PORT, BSP_W25Q128_CS_PIN, GPIO_PIN_RESET)
#define BSP_W25Q128_CS_HIGH()           HAL_GPIO_WritePin(BSP_W25Q128_CS_PORT, BSP_W25Q128_CS_PIN, GPIO_PIN_SET)

static bsp_w25q128_status_t bsp_w25q128_check_address(uint32_t address, uint32_t length)
{
    if (length == 0U)
    {
        return BSP_W25Q128_OK;
    }

    if (address >= BSP_W25Q128_FLASH_SIZE)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    if (length > (BSP_W25Q128_FLASH_SIZE - address))
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    return BSP_W25Q128_OK;
}

static bsp_w25q128_status_t bsp_w25q128_hal_to_status(HAL_StatusTypeDef hal_status)
{
    switch (hal_status)
    {
    case HAL_OK:
        return BSP_W25Q128_OK;
    case HAL_TIMEOUT:
        return BSP_W25Q128_TIMEOUT;
    default:
        return BSP_W25Q128_ERROR;
    }
}

static bsp_w25q128_status_t bsp_w25q128_tx(const uint8_t *data, uint16_t size)
{
    if ((data == NULL) && (size > 0U))
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    return bsp_w25q128_hal_to_status(HAL_SPI_Transmit(&hspi5, (uint8_t *)data, size, BSP_W25Q128_SPI_TIMEOUT_MS));
}

static bsp_w25q128_status_t bsp_w25q128_rx(uint8_t *buf, uint16_t len)
{
    uint8_t dummy[256];

    while (len)
    {
        uint16_t chunk = (len > sizeof(dummy)) ? sizeof(dummy) : len;

        memset(dummy, 0xFF, chunk);

        if (HAL_SPI_TransmitReceive(&hspi5, dummy, buf, chunk, 100U) != HAL_OK)
        {
            return BSP_W25Q128_ERROR;
        }

        buf += chunk;
        len -= chunk;
    }

    return BSP_W25Q128_OK;
}

static uint8_t bsp_w25q128_read_status(uint8_t cmd)
{
    uint8_t reg = 0U;

    BSP_W25Q128_CS_LOW();
    (void)bsp_w25q128_tx(&cmd, 1U);
    (void)bsp_w25q128_rx(&reg, 1U);
    BSP_W25Q128_CS_HIGH();

    return reg;
}

static bsp_w25q128_status_t bsp_w25q128_send_command(uint8_t cmd)
{
    bsp_w25q128_status_t status;

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(&cmd, 1U);
    BSP_W25Q128_CS_HIGH();

    return status;
}

static bsp_w25q128_status_t bsp_w25q128_send_command_addr(uint8_t cmd, uint32_t address)
{
    uint8_t tx[4];
    bsp_w25q128_status_t status;

    tx[0] = cmd;
    tx[1] = (uint8_t)(address >> 16);
    tx[2] = (uint8_t)(address >> 8);
    tx[3] = (uint8_t)(address >> 0);

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(tx, sizeof(tx));
    BSP_W25Q128_CS_HIGH();

    return status;
}

bsp_w25q128_status_t BSP_W25Q128_Init(void)
{
    BSP_W25Q128_CS_HIGH();
    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_Reset(void)
{
    bsp_w25q128_status_t status;

    status = bsp_w25q128_send_command(BSP_W25Q128_CMD_ENABLE_RESET);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = bsp_w25q128_send_command(BSP_W25Q128_CMD_RESET_DEVICE);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    HAL_Delay(1U);
    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_ReadID(bsp_w25q128_id_t *id)
{
    uint8_t cmd = BSP_W25Q128_CMD_JEDEC_ID;
    uint8_t rx[3] = {0U};
    bsp_w25q128_status_t status;

    if (id == NULL)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(&cmd, 1U);
    if (status == BSP_W25Q128_OK)
    {
        status = bsp_w25q128_rx(rx, sizeof(rx));
    }
    BSP_W25Q128_CS_HIGH();

    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    id->manufacturer_id = rx[0];
    id->memory_type = rx[1];
    id->capacity_id = rx[2];

    return BSP_W25Q128_OK;
}

bool BSP_W25Q128_IsPresent(void)
{
    bsp_w25q128_id_t id;

    if (BSP_W25Q128_ReadID(&id) != BSP_W25Q128_OK)
    {
        return false;
    }

    if (id.manufacturer_id != BSP_W25Q128_JEDEC_MID)
    {
        return false;
    }

    if ((id.memory_type != BSP_W25Q128_JEDEC_MEMTYPE_A) &&
        (id.memory_type != BSP_W25Q128_JEDEC_MEMTYPE_B))
    {
        return false;
    }

    return (id.capacity_id == BSP_W25Q128_JEDEC_CAPACITY);
}

bsp_w25q128_status_t BSP_W25Q128_ReadUniqueID(uint8_t uid[8])
{
    uint8_t header[5] = {BSP_W25Q128_CMD_READ_UNIQUE_ID, 0x00U, 0x00U, 0x00U, 0x00U};
    bsp_w25q128_status_t status;

    if (uid == NULL)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(header, sizeof(header));
    if (status == BSP_W25Q128_OK)
    {
        status = bsp_w25q128_rx(uid, 8U);
    }
    BSP_W25Q128_CS_HIGH();

    return status;
}

uint8_t BSP_W25Q128_ReadStatus1(void)
{
    return bsp_w25q128_read_status(BSP_W25Q128_CMD_READ_STATUS1);
}

uint8_t BSP_W25Q128_ReadStatus2(void)
{
    return bsp_w25q128_read_status(BSP_W25Q128_CMD_READ_STATUS2);
}

uint8_t BSP_W25Q128_ReadStatus3(void)
{
    return bsp_w25q128_read_status(BSP_W25Q128_CMD_READ_STATUS3);
}

bool BSP_W25Q128_IsBusy(void)
{
    return ((BSP_W25Q128_ReadStatus1() & BSP_W25Q128_SR1_BUSY) != 0U);
}

bsp_w25q128_status_t BSP_W25Q128_WaitWhileBusy(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (BSP_W25Q128_IsBusy())
    {
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return BSP_W25Q128_TIMEOUT;
        }
    }

    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_WriteEnable(void)
{
    bsp_w25q128_status_t status;
    uint32_t start;

    status = bsp_w25q128_send_command(BSP_W25Q128_CMD_WRITE_ENABLE);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    start = HAL_GetTick();
    while ((BSP_W25Q128_ReadStatus1() & BSP_W25Q128_SR1_WEL) == 0U)
    {
        if ((HAL_GetTick() - start) >= BSP_W25Q128_SPI_TIMEOUT_MS)
        {
            return BSP_W25Q128_TIMEOUT;
        }
    }

    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_WriteDisable(void)
{
    return bsp_w25q128_send_command(BSP_W25Q128_CMD_WRITE_DISABLE);
}

bsp_w25q128_status_t BSP_W25Q128_Read(uint32_t address, uint8_t *buffer, uint32_t length)
{
    uint8_t header[4];
    bsp_w25q128_status_t status;

    status = bsp_w25q128_check_address(address, length);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    if ((buffer == NULL) && (length > 0U))
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    if (length == 0U)
    {
        return BSP_W25Q128_OK;
    }

    header[0] = BSP_W25Q128_CMD_READ_DATA;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)(address >> 0);

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(header, sizeof(header));
    while ((status == BSP_W25Q128_OK) && (length > 0U))
    {
        uint16_t chunk = (length > 65535UL) ? 65535U : (uint16_t)length;

        status = bsp_w25q128_rx(buffer, chunk);
        buffer += chunk;
        length -= chunk;
    }
    BSP_W25Q128_CS_HIGH();

    return status;
}

bsp_w25q128_status_t BSP_W25Q128_FastRead(uint32_t address, uint8_t *buffer, uint32_t length)
{
    uint8_t header[5];
    bsp_w25q128_status_t status;

    status = bsp_w25q128_check_address(address, length);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    if ((buffer == NULL) && (length > 0U))
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    if (length == 0U)
    {
        return BSP_W25Q128_OK;
    }

    header[0] = BSP_W25Q128_CMD_FAST_READ;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)(address >> 0);
    header[4] = 0x00U;

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(header, sizeof(header));
    while ((status == BSP_W25Q128_OK) && (length > 0U))
    {
        uint16_t chunk = (length > 65535UL) ? 65535U : (uint16_t)length;

        status = bsp_w25q128_rx(buffer, chunk);
        buffer += chunk;
        length -= chunk;
    }
    BSP_W25Q128_CS_HIGH();

    return status;
}

bsp_w25q128_status_t BSP_W25Q128_PageProgram(uint32_t address, const uint8_t *buffer, uint16_t length)
{
    uint8_t header[4];
    uint32_t page_offset;
    bsp_w25q128_status_t status;

    status = bsp_w25q128_check_address(address, length);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    if ((buffer == NULL) && (length > 0U))
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    if (length == 0U)
    {
        return BSP_W25Q128_OK;
    }

    if (length > BSP_W25Q128_PAGE_SIZE)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    page_offset = address % BSP_W25Q128_PAGE_SIZE;
    if ((page_offset + length) > BSP_W25Q128_PAGE_SIZE)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    status = BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WriteEnable();
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    header[0] = BSP_W25Q128_CMD_PAGE_PROGRAM;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)(address >> 0);

    BSP_W25Q128_CS_LOW();
    status = bsp_w25q128_tx(header, sizeof(header));
    if (status == BSP_W25Q128_OK)
    {
        status = bsp_w25q128_tx(buffer, length);
    }
    BSP_W25Q128_CS_HIGH();

    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    return BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_Write(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    bsp_w25q128_status_t status;

    status = bsp_w25q128_check_address(address, length);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    if ((buffer == NULL) && (length > 0U))
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    while (length > 0U)
    {
        uint32_t page_offset = address % BSP_W25Q128_PAGE_SIZE;
        uint32_t chunk = BSP_W25Q128_PAGE_SIZE - page_offset;

        if (chunk > length)
        {
            chunk = length;
        }

        status = BSP_W25Q128_PageProgram(address, buffer, (uint16_t)chunk);
        if (status != BSP_W25Q128_OK)
        {
            return status;
        }

        address += chunk;
        buffer += chunk;
        length -= chunk;
    }

    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_EraseSector(uint32_t address)
{
    bsp_w25q128_status_t status;

    if (address >= BSP_W25Q128_FLASH_SIZE)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    address &= ~(BSP_W25Q128_SECTOR_SIZE - 1UL);

    status = BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WriteEnable();
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = bsp_w25q128_send_command_addr(BSP_W25Q128_CMD_SECTOR_ERASE_4K, address);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    return BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_Erase32Block(uint32_t address)
{
    bsp_w25q128_status_t status;

    if (address >= BSP_W25Q128_FLASH_SIZE)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    address &= ~(BSP_W25Q128_BLOCK32_SIZE - 1UL);

    status = BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WriteEnable();
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = bsp_w25q128_send_command_addr(BSP_W25Q128_CMD_BLOCK_ERASE_32K, address);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    return BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_Erase64Block(uint32_t address)
{
    bsp_w25q128_status_t status;

    if (address >= BSP_W25Q128_FLASH_SIZE)
    {
        return BSP_W25Q128_PARAM_ERROR;
    }

    address &= ~(BSP_W25Q128_BLOCK64_SIZE - 1UL);

    status = BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WriteEnable();
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = bsp_w25q128_send_command_addr(BSP_W25Q128_CMD_BLOCK_ERASE_64K, address);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    return BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_ChipErase(void)
{
    bsp_w25q128_status_t status;

    status = BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = BSP_W25Q128_WriteEnable();
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    status = bsp_w25q128_send_command(BSP_W25Q128_CMD_CHIP_ERASE_1);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    return BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_CHIP_ERASE_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_PowerDown(void)
{
    bsp_w25q128_status_t status;

    status = BSP_W25Q128_WaitWhileBusy(BSP_W25Q128_BUSY_TIMEOUT_MS);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    return bsp_w25q128_send_command(BSP_W25Q128_CMD_POWER_DOWN);
}

bsp_w25q128_status_t BSP_W25Q128_ReleasePowerDown(void)
{
    bsp_w25q128_status_t status;

    status = bsp_w25q128_send_command(BSP_W25Q128_CMD_RELEASE_POWER_DOWN);
    if (status != BSP_W25Q128_OK)
    {
        return status;
    }

    HAL_Delay(1U);
    return BSP_W25Q128_OK;
}
