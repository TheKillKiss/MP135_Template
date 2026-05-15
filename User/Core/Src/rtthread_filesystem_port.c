#include <dfs_fs.h>
#include <drivers/classes/block.h>
#include <drivers/mtd_nor.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <string.h>

#ifdef BSP_USING_EMMC
#include "bsp_emmc.h"
#define EMMC_BLOCK_DEVICE_NAME          "emmc0"
#else
#undef BSP_USING_FS_AUTO_MOUNT_EMMC
#endif

#ifdef BSP_USING_W25Q128
#include "bsp_spi.h"
#include "bsp_w25q128.h"
#define W25Q_MTD_DEVICE_NAME            "w25q"
#else
#undef BSP_USING_W25Q128_BLOCK
#endif

#ifdef BSP_USING_W25Q128_BLOCK
#define W25Q_BLOCK_DEVICE_NAME          "w25q0"
#define W25Q_LOGICAL_SECTOR_SIZE        512U
#define W25Q_ERASE_BLOCK_SIZE           BSP_W25Q128_SECTOR_SIZE
#define W25Q_SECTORS_PER_ERASE_BLOCK    (W25Q_ERASE_BLOCK_SIZE / W25Q_LOGICAL_SECTOR_SIZE)
#else
#undef BSP_USING_FS_AUTO_MOUNT_W25Q
#endif

#if defined(BSP_USING_FS_AUTO_MOUNT_EMMC) && defined(BSP_USING_FS_AUTO_MOUNT_W25Q)
#error "Only one root filesystem auto mount device can be enabled"
#endif

#ifdef BSP_USING_EMMC
static struct rt_device emmc_device;
#endif

#ifdef BSP_USING_W25Q128_BLOCK
static struct rt_device w25q_block_device;
#endif

#ifdef BSP_USING_W25Q128
static struct rt_mtd_nor_device w25q_mtd_device;
static struct rt_mutex w25q_lock;
#endif

#ifdef BSP_USING_W25Q128_BLOCK
static rt_uint8_t w25q_cache[W25Q_ERASE_BLOCK_SIZE] __attribute__((aligned(32)));
#endif

#ifdef BSP_USING_EMMC
static rt_err_t emmc_init(rt_device_t dev)
{
    RT_UNUSED(dev);
    return RT_EOK;
}

static rt_err_t emmc_open(rt_device_t dev, rt_uint16_t oflag)
{
    RT_UNUSED(dev);
    RT_UNUSED(oflag);
    return RT_EOK;
}

static rt_err_t emmc_close(rt_device_t dev)
{
    RT_UNUSED(dev);
    return RT_EOK;
}

static rt_ssize_t emmc_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    if (BSP_EMMC_ReadBlocks((uint32_t)pos, (uint8_t *)buffer, (uint32_t)size) != HAL_OK)
    {
        return 0;
    }

    return (rt_ssize_t)size;
}

static rt_ssize_t emmc_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    if (BSP_EMMC_WriteBlocks((uint32_t)pos, (const uint8_t *)buffer, (uint32_t)size) != HAL_OK)
    {
        return 0;
    }

    return (rt_ssize_t)size;
}

static rt_err_t emmc_control(rt_device_t dev, int cmd, void *args)
{
    HAL_MMC_CardInfoTypeDef card_info;

    RT_UNUSED(dev);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_BLK_GETGEOME:
    {
        struct rt_device_blk_geometry *geometry = (struct rt_device_blk_geometry *)args;

        if (geometry == RT_NULL)
        {
            return -RT_EINVAL;
        }

        if (BSP_EMMC_GetCardInfo(&card_info) != HAL_OK)
        {
            return -RT_EIO;
        }

        geometry->bytes_per_sector = MMC_BLOCKSIZE;
        geometry->sector_count = card_info.LogBlockNbr;
        geometry->block_size = MMC_BLOCKSIZE;
        return RT_EOK;
    }

    case RT_DEVICE_CTRL_BLK_SYNC:
        return RT_EOK;

    default:
        return -RT_ENOSYS;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops emmc_ops =
{
    emmc_init,
    emmc_open,
    emmc_close,
    emmc_read,
    emmc_write,
    emmc_control
};
#endif
#endif

#ifdef BSP_USING_W25Q128
static rt_err_t w25q_lock_take(void)
{
    return rt_mutex_take(&w25q_lock, RT_WAITING_FOREVER);
}

static void w25q_lock_release(void)
{
    rt_mutex_release(&w25q_lock);
}

static rt_err_t w25q_mtd_read_id(struct rt_mtd_nor_device *device)
{
    bsp_w25q128_id_t id;

    RT_UNUSED(device);

    if (BSP_W25Q128_ReadID(&id) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    return (rt_err_t)(((rt_uint32_t)id.manufacturer_id << 16) |
                      ((rt_uint32_t)id.memory_type << 8) |
                      ((rt_uint32_t)id.capacity_id));
}

static rt_ssize_t w25q_mtd_read(struct rt_mtd_nor_device *device,
                                rt_off_t offset,
                                rt_uint8_t *data,
                                rt_size_t length)
{
    RT_UNUSED(device);

    if ((data == RT_NULL) || (length == 0U))
    {
        return 0;
    }

    if ((offset < 0) || (((rt_uint32_t)offset + length) > BSP_W25Q128_FLASH_SIZE))
    {
        return -RT_EINVAL;
    }

    if (w25q_lock_take() != RT_EOK)
    {
        return -RT_EBUSY;
    }

    if (BSP_W25Q128_Read((rt_uint32_t)offset, data, (rt_uint32_t)length) != BSP_W25Q128_OK)
    {
        w25q_lock_release();
        return -RT_EIO;
    }

    w25q_lock_release();
    return (rt_ssize_t)length;
}

static rt_ssize_t w25q_mtd_write(struct rt_mtd_nor_device *device,
                                 rt_off_t offset,
                                 const rt_uint8_t *data,
                                 rt_size_t length)
{
    RT_UNUSED(device);

    if ((data == RT_NULL) || (length == 0U))
    {
        return 0;
    }

    if ((offset < 0) || (((rt_uint32_t)offset + length) > BSP_W25Q128_FLASH_SIZE))
    {
        return -RT_EINVAL;
    }

    if (w25q_lock_take() != RT_EOK)
    {
        return -RT_EBUSY;
    }

    if (BSP_W25Q128_Write((rt_uint32_t)offset, data, (rt_uint32_t)length) != BSP_W25Q128_OK)
    {
        w25q_lock_release();
        return -RT_EIO;
    }

    w25q_lock_release();
    return (rt_ssize_t)length;
}

static rt_err_t w25q_mtd_erase_block(struct rt_mtd_nor_device *device,
                                     rt_off_t offset,
                                     rt_size_t length)
{
    rt_uint32_t address;
    rt_uint32_t end;

    RT_UNUSED(device);

    if ((offset < 0) || ((offset % BSP_W25Q128_SECTOR_SIZE) != 0) ||
        ((length % BSP_W25Q128_SECTOR_SIZE) != 0))
    {
        return -RT_EINVAL;
    }

    address = (rt_uint32_t)offset;
    end = address + length;
    if (end > BSP_W25Q128_FLASH_SIZE)
    {
        return -RT_EINVAL;
    }

    if (w25q_lock_take() != RT_EOK)
    {
        return -RT_EBUSY;
    }

    while (address < end)
    {
        if (BSP_W25Q128_EraseSector(address) != BSP_W25Q128_OK)
        {
            w25q_lock_release();
            return -RT_EIO;
        }

        address += BSP_W25Q128_SECTOR_SIZE;
    }

    w25q_lock_release();
    return RT_EOK;
}

static const struct rt_mtd_nor_driver_ops w25q_mtd_ops =
{
    w25q_mtd_read_id,
    w25q_mtd_read,
    w25q_mtd_write,
    w25q_mtd_erase_block
};
#endif

#ifdef BSP_USING_W25Q128_BLOCK
static rt_err_t w25q_block_init(rt_device_t dev)
{
    RT_UNUSED(dev);
    return RT_EOK;
}

static rt_err_t w25q_block_open(rt_device_t dev, rt_uint16_t oflag)
{
    RT_UNUSED(dev);
    RT_UNUSED(oflag);
    return RT_EOK;
}

static rt_err_t w25q_block_close(rt_device_t dev)
{
    RT_UNUSED(dev);
    return RT_EOK;
}

static rt_ssize_t w25q_block_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_uint32_t address;
    rt_uint32_t length;

    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    address = (rt_uint32_t)pos * W25Q_LOGICAL_SECTOR_SIZE;
    length = (rt_uint32_t)size * W25Q_LOGICAL_SECTOR_SIZE;

    if ((address + length) > BSP_W25Q128_FLASH_SIZE)
    {
        return 0;
    }

    if (w25q_mtd_read(&w25q_mtd_device, address, (rt_uint8_t *)buffer, length) != (rt_ssize_t)length)
    {
        return 0;
    }

    return (rt_ssize_t)size;
}

static rt_err_t w25q_block_write_one_erase_block(rt_uint32_t erase_block_index,
                                                 rt_uint32_t start_sector_in_block,
                                                 const rt_uint8_t *buffer,
                                                 rt_uint32_t sector_count)
{
    rt_uint32_t erase_address = erase_block_index * W25Q_ERASE_BLOCK_SIZE;
    rt_uint32_t sector_offset = start_sector_in_block * W25Q_LOGICAL_SECTOR_SIZE;
    rt_uint32_t length = sector_count * W25Q_LOGICAL_SECTOR_SIZE;

    if (BSP_W25Q128_Read(erase_address, w25q_cache, W25Q_ERASE_BLOCK_SIZE) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    memcpy(&w25q_cache[sector_offset], buffer, length);

    if (BSP_W25Q128_EraseSector(erase_address) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    if (BSP_W25Q128_Write(erase_address, w25q_cache, W25Q_ERASE_BLOCK_SIZE) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    return RT_EOK;
}

static rt_ssize_t w25q_block_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_uint32_t sector = (rt_uint32_t)pos;
    rt_uint32_t remain = (rt_uint32_t)size;
    const rt_uint8_t *src = (const rt_uint8_t *)buffer;

    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    if (((rt_uint32_t)pos + size) > (BSP_W25Q128_FLASH_SIZE / W25Q_LOGICAL_SECTOR_SIZE))
    {
        return 0;
    }

    if (w25q_lock_take() != RT_EOK)
    {
        return 0;
    }

    while (remain > 0U)
    {
        rt_uint32_t erase_block_index = sector / W25Q_SECTORS_PER_ERASE_BLOCK;
        rt_uint32_t sector_in_block = sector % W25Q_SECTORS_PER_ERASE_BLOCK;
        rt_uint32_t chunk = W25Q_SECTORS_PER_ERASE_BLOCK - sector_in_block;

        if (chunk > remain)
        {
            chunk = remain;
        }

        if (w25q_block_write_one_erase_block(erase_block_index, sector_in_block, src, chunk) != RT_EOK)
        {
            w25q_lock_release();
            return 0;
        }

        sector += chunk;
        src += chunk * W25Q_LOGICAL_SECTOR_SIZE;
        remain -= chunk;
    }

    w25q_lock_release();
    return (rt_ssize_t)size;
}

static rt_err_t w25q_block_control(rt_device_t dev, int cmd, void *args)
{
    RT_UNUSED(dev);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_BLK_GETGEOME:
    {
        struct rt_device_blk_geometry *geometry = (struct rt_device_blk_geometry *)args;

        if (geometry == RT_NULL)
        {
            return -RT_EINVAL;
        }

        geometry->bytes_per_sector = W25Q_LOGICAL_SECTOR_SIZE;
        geometry->sector_count = BSP_W25Q128_FLASH_SIZE / W25Q_LOGICAL_SECTOR_SIZE;
        geometry->block_size = W25Q_ERASE_BLOCK_SIZE;
        return RT_EOK;
    }

    case RT_DEVICE_CTRL_BLK_SYNC:
        return RT_EOK;

    default:
        return -RT_ENOSYS;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops w25q_block_ops =
{
    w25q_block_init,
    w25q_block_open,
    w25q_block_close,
    w25q_block_read,
    w25q_block_write,
    w25q_block_control
};
#endif
#endif

static int rt_hw_storage_device_init(void)
{
#if defined(BSP_USING_EMMC) || defined(BSP_USING_W25Q128)
    rt_err_t result;
#endif

#ifdef BSP_USING_EMMC
    BSP_EMMC_Init();
    emmc_device.type = RT_Device_Class_Block;
#ifdef RT_USING_DEVICE_OPS
    emmc_device.ops = &emmc_ops;
#else
    emmc_device.init = emmc_init;
    emmc_device.open = emmc_open;
    emmc_device.close = emmc_close;
    emmc_device.read = emmc_read;
    emmc_device.write = emmc_write;
    emmc_device.control = emmc_control;
#endif
    result = rt_device_register(&emmc_device, EMMC_BLOCK_DEVICE_NAME, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
    if (result != RT_EOK)
    {
        rt_kprintf("emmc register failed: %d\n", result);
    }
#endif

#ifdef BSP_USING_W25Q128
    BSP_SPI5_Init();
    (void)BSP_W25Q128_Init();
    (void)BSP_W25Q128_Reset();

    if (rt_mutex_init(&w25q_lock, "w25qlk", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (BSP_W25Q128_IsPresent())
    {
        w25q_mtd_device.block_size = BSP_W25Q128_SECTOR_SIZE;
        w25q_mtd_device.block_start = 0U;
        w25q_mtd_device.block_end = BSP_W25Q128_SECTOR_COUNT;
        w25q_mtd_device.ops = &w25q_mtd_ops;

        result = rt_mtd_nor_register_device(W25Q_MTD_DEVICE_NAME, &w25q_mtd_device);
        if (result != RT_EOK)
        {
            rt_kprintf("w25q mtd register failed: %d\n", result);
        }

#ifdef BSP_USING_W25Q128_BLOCK
        w25q_block_device.type = RT_Device_Class_Block;
#ifdef RT_USING_DEVICE_OPS
        w25q_block_device.ops = &w25q_block_ops;
#else
        w25q_block_device.init = w25q_block_init;
        w25q_block_device.open = w25q_block_open;
        w25q_block_device.close = w25q_block_close;
        w25q_block_device.read = w25q_block_read;
        w25q_block_device.write = w25q_block_write;
        w25q_block_device.control = w25q_block_control;
#endif
        result = rt_device_register(&w25q_block_device, W25Q_BLOCK_DEVICE_NAME,
                                    RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
        if (result != RT_EOK)
        {
            rt_kprintf("w25q block register failed: %d\n", result);
        }
#endif
    }
    else
    {
        rt_kprintf("w25q128 not found\n");
    }
#endif

    return 0;
}
INIT_BOARD_EXPORT(rt_hw_storage_device_init);

static int rt_hw_filesystem_mount(void)
{
#if defined(BSP_USING_EMMC) && defined(BSP_USING_FS_AUTO_MOUNT_EMMC)
    if (dfs_mount(EMMC_BLOCK_DEVICE_NAME, "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("emmc mounted on /\n");
    }
    else
    {
        rt_kprintf("emmc mount failed, run: mkfs -t elm %s\n", EMMC_BLOCK_DEVICE_NAME);
    }
#elif defined(BSP_USING_W25Q128_BLOCK) && defined(BSP_USING_FS_AUTO_MOUNT_W25Q)
    if (dfs_mount(W25Q_BLOCK_DEVICE_NAME, "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("w25q mounted on /\n");
    }
    else
    {
        rt_kprintf("w25q mount failed, run: mkfs -t elm %s\n", W25Q_BLOCK_DEVICE_NAME);
    }
#endif

    return 0;
}
INIT_APP_EXPORT(rt_hw_filesystem_mount);
