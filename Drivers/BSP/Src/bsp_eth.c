#include "bsp_eth.h"

#ifdef BSP_USING_ETH

#include "bsp_yt8531c.h"
#include "cachel1_armv7.h"
#include "irq_ctrl.h"
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/ethernetif.h>
#include <stddef.h>
#include <string.h>

#ifndef BSP_ETH_IRQ_PRIORITY
#define BSP_ETH_IRQ_PRIORITY            (0xA0U)
#endif

#define BSP_ETH_DEVICE_NAME             "e0"
#define BSP_ETH_MAC_ADDR0               0x00U
#define BSP_ETH_MAC_ADDR1               0x80U
#define BSP_ETH_MAC_ADDR2               0xE1U
#define BSP_ETH_MAC_ADDR3               0x00U
#define BSP_ETH_MAC_ADDR4               0x00U
#define BSP_ETH_MAC_ADDR5               0x00U

#define BSP_ETH_TX_BUFFER_SIZE          1536U
#define BSP_ETH_RX_BUFFER_SIZE          1536U
#define BSP_ETH_RX_BUFFER_COUNT         12U

typedef struct
{
    struct pbuf_custom pbuf_custom;
    rt_uint8_t buff[(BSP_ETH_RX_BUFFER_SIZE + 31U) & ~31U] __attribute__((aligned(32)));
} bsp_eth_rx_buff_t;

typedef enum
{
    BSP_ETH_RX_ALLOC_OK = 0,
    BSP_ETH_RX_ALLOC_ERROR
} bsp_eth_rx_alloc_status_t;

ETH_HandleTypeDef heth1;
ETH_TxPacketConfigTypeDef TxConfig;

static ETH_DMADescTypeDef eth_rx_desc[ETH_RX_DESC_CNT] __attribute__((aligned(32)));
static ETH_DMADescTypeDef eth_tx_desc[ETH_TX_DESC_CNT] __attribute__((aligned(32)));
static rt_uint8_t eth_tx_buffer[BSP_ETH_TX_BUFFER_SIZE] __attribute__((aligned(32)));
static struct eth_device eth_device;
static yt8531c_Object_t yt8531c;
static rt_thread_t eth_link_thread;
static rt_uint8_t eth_mac_addr[6] =
{
    BSP_ETH_MAC_ADDR0,
    BSP_ETH_MAC_ADDR1,
    BSP_ETH_MAC_ADDR2,
    BSP_ETH_MAC_ADDR3,
    BSP_ETH_MAC_ADDR4,
    BSP_ETH_MAC_ADDR5
};
static volatile bsp_eth_rx_alloc_status_t eth_rx_alloc_status = BSP_ETH_RX_ALLOC_OK;
static int32_t eth_applied_link_state = YT8531C_STATUS_LINK_DOWN;
static rt_bool_t eth_started = RT_FALSE;

LWIP_MEMPOOL_DECLARE(ETH_RX_POOL,
                     BSP_ETH_RX_BUFFER_COUNT,
                     sizeof(bsp_eth_rx_buff_t),
                     "ETH RX PBUF pool");

static void bsp_eth_irq_handler(void);
static void bsp_eth_pbuf_free_custom(struct pbuf *p);

#ifdef CACHE_USE
static void bsp_eth_cache_clean(const void *addr, rt_size_t size)
{
    rt_ubase_t start;
    rt_ubase_t end;

    if ((addr == RT_NULL) || (size == 0U))
    {
        return;
    }

    start = ((rt_ubase_t)addr) & ~(rt_ubase_t)31U;
    end = (((rt_ubase_t)addr) + size + 31U) & ~(rt_ubase_t)31U;
    SCB_CleanDCache_by_Addr((void *)start, (int32_t)(end - start));
}

static void bsp_eth_cache_invalidate(const void *addr, rt_size_t size)
{
    rt_ubase_t start;
    rt_ubase_t end;

    if ((addr == RT_NULL) || (size == 0U))
    {
        return;
    }

    start = ((rt_ubase_t)addr) & ~(rt_ubase_t)31U;
    end = (((rt_ubase_t)addr) + size + 31U) & ~(rt_ubase_t)31U;
    SCB_InvalidateDCache_by_Addr((void *)start, (int32_t)(end - start));
}
#else
#define bsp_eth_cache_clean(addr, size)       do { (void)(addr); (void)(size); } while (0)
#define bsp_eth_cache_invalidate(addr, size)  do { (void)(addr); (void)(size); } while (0)
#endif

static rt_bool_t bsp_eth_is_valid_link_state(int32_t link_state)
{
    return (rt_bool_t)((link_state == YT8531C_STATUS_10MBITS_FULLDUPLEX) ||
                       (link_state == YT8531C_STATUS_10MBITS_HALFDUPLEX) ||
                       (link_state == YT8531C_STATUS_100MBITS_FULLDUPLEX) ||
                       (link_state == YT8531C_STATUS_100MBITS_HALFDUPLEX) ||
                       (link_state == YT8531C_STATUS_1000MBITS_FULLDUPLEX) ||
                       (link_state == YT8531C_STATUS_1000MBITS_HALFDUPLEX));
}

static HAL_StatusTypeDef bsp_eth_apply_mac_config(int32_t link_state)
{
    ETH_MACConfigTypeDef mac_config;

    if (HAL_ETH_GetMACConfig(&heth1, &mac_config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    switch (link_state)
    {
    case YT8531C_STATUS_10MBITS_FULLDUPLEX:
        mac_config.Speed = ETH_SPEED_10M;
        mac_config.DuplexMode = ETH_FULLDUPLEX_MODE;
        break;
    case YT8531C_STATUS_10MBITS_HALFDUPLEX:
        mac_config.Speed = ETH_SPEED_10M;
        mac_config.DuplexMode = ETH_HALFDUPLEX_MODE;
        break;
    case YT8531C_STATUS_100MBITS_FULLDUPLEX:
        mac_config.Speed = ETH_SPEED_100M;
        mac_config.DuplexMode = ETH_FULLDUPLEX_MODE;
        break;
    case YT8531C_STATUS_100MBITS_HALFDUPLEX:
        mac_config.Speed = ETH_SPEED_100M;
        mac_config.DuplexMode = ETH_HALFDUPLEX_MODE;
        break;
    case YT8531C_STATUS_1000MBITS_FULLDUPLEX:
        mac_config.Speed = ETH_SPEED_1000M;
        mac_config.DuplexMode = ETH_FULLDUPLEX_MODE;
        break;
    case YT8531C_STATUS_1000MBITS_HALFDUPLEX:
        mac_config.Speed = ETH_SPEED_1000M;
        mac_config.DuplexMode = ETH_HALFDUPLEX_MODE;
        break;
    default:
        return HAL_ERROR;
    }

    if (HAL_ETH_SetMACConfig(&heth1, &mac_config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    eth_applied_link_state = link_state;
    return HAL_OK;
}

static int32_t bsp_eth_phy_io_init(void)
{
    return YT8531C_STATUS_OK;
}

static int32_t bsp_eth_phy_io_deinit(void)
{
    return YT8531C_STATUS_OK;
}

static int32_t bsp_eth_phy_io_read_reg(uint32_t dev_addr, uint32_t reg, uint32_t *value)
{
    if (HAL_ETH_ReadPHYRegister(&heth1, dev_addr, reg, value) != HAL_OK)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    return YT8531C_STATUS_OK;
}

static int32_t bsp_eth_phy_io_write_reg(uint32_t dev_addr, uint32_t reg, uint32_t value)
{
    if (HAL_ETH_WritePHYRegister(&heth1, dev_addr, reg, value) != HAL_OK)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

static int32_t bsp_eth_phy_io_get_tick(void)
{
    return (int32_t)HAL_GetTick();
}

static rt_err_t bsp_eth_phy_init(void)
{
    yt8531c_IOCtx_t ioctx;

    memset(&ioctx, 0, sizeof(ioctx));
    ioctx.Init = bsp_eth_phy_io_init;
    ioctx.DeInit = bsp_eth_phy_io_deinit;
    ioctx.ReadReg = bsp_eth_phy_io_read_reg;
    ioctx.WriteReg = bsp_eth_phy_io_write_reg;
    ioctx.GetTick = bsp_eth_phy_io_get_tick;

    if (YT8531C_RegisterBusIO(&yt8531c, &ioctx) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_Init(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_XtalInit() != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_ConfigRGMII_Delay(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }

    YT8531C_LED_Init();

    if (YT8531C_DisablePowerDownMode(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_StartAutoNego(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void bsp_eth_update_link(void)
{
    int32_t link_state;

    link_state = YT8531C_GetLinkState(&yt8531c);
    if (bsp_eth_is_valid_link_state(link_state))
    {
        if ((eth_started == RT_FALSE) || (eth_applied_link_state != link_state))
        {
            (void)HAL_ETH_Stop_IT(&heth1);
            if (bsp_eth_apply_mac_config(link_state) == HAL_OK)
            {
                if (HAL_ETH_Start_IT(&heth1) == HAL_OK)
                {
                    eth_started = RT_TRUE;
                    (void)eth_device_linkchange(&eth_device, RT_TRUE);
                    rt_kprintf("eth link up: %d\n", link_state);
                }
            }
        }
    }
    else
    {
        if (eth_started == RT_TRUE)
        {
            (void)HAL_ETH_Stop_IT(&heth1);
            eth_started = RT_FALSE;
            (void)eth_device_linkchange(&eth_device, RT_FALSE);
            rt_kprintf("eth link down\n");
        }
        eth_applied_link_state = YT8531C_STATUS_LINK_DOWN;
    }
}

static void bsp_eth_link_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        bsp_eth_update_link();
        rt_thread_mdelay(1000);
    }
}

static rt_err_t bsp_eth_dev_init(rt_device_t dev)
{
    RT_UNUSED(dev);
    return RT_EOK;
}

static rt_err_t bsp_eth_dev_open(rt_device_t dev, rt_uint16_t oflag)
{
    RT_UNUSED(dev);
    RT_UNUSED(oflag);
    return RT_EOK;
}

static rt_err_t bsp_eth_dev_close(rt_device_t dev)
{
    RT_UNUSED(dev);
    return RT_EOK;
}

static rt_ssize_t bsp_eth_dev_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(pos);
    RT_UNUSED(buffer);
    RT_UNUSED(size);
    return 0;
}

static rt_ssize_t bsp_eth_dev_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(pos);
    RT_UNUSED(buffer);
    RT_UNUSED(size);
    return 0;
}

static rt_err_t bsp_eth_dev_control(rt_device_t dev, int cmd, void *args)
{
    RT_UNUSED(dev);

    switch (cmd)
    {
    case NIOCTL_GADDR:
        if (args == RT_NULL)
        {
            return -RT_EINVAL;
        }
        memcpy(args, eth_mac_addr, sizeof(eth_mac_addr));
        return RT_EOK;

    default:
        return -RT_ENOSYS;
    }
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops bsp_eth_ops =
{
    bsp_eth_dev_init,
    bsp_eth_dev_open,
    bsp_eth_dev_close,
    bsp_eth_dev_read,
    bsp_eth_dev_write,
    bsp_eth_dev_control
};
#endif

static rt_err_t bsp_eth_tx(rt_device_t dev, struct pbuf *p)
{
    struct pbuf *q;
    rt_uint32_t frame_len = 0;
    ETH_BufferTypeDef tx_buffer;
    rt_err_t result = RT_EOK;

    RT_UNUSED(dev);

    if ((p == RT_NULL) || (p->tot_len > BSP_ETH_TX_BUFFER_SIZE) || (eth_started == RT_FALSE))
    {
        return -RT_ERROR;
    }

#if ETH_PAD_SIZE
    if (pbuf_remove_header(p, ETH_PAD_SIZE) != 0)
    {
        return -RT_ERROR;
    }
#endif

    if (p->tot_len > BSP_ETH_TX_BUFFER_SIZE)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    for (q = p; q != RT_NULL; q = q->next)
    {
        memcpy(&eth_tx_buffer[frame_len], q->payload, q->len);
        frame_len += q->len;
    }

    bsp_eth_cache_clean(eth_tx_buffer, frame_len);

    memset(&tx_buffer, 0, sizeof(tx_buffer));
    tx_buffer.buffer = eth_tx_buffer;
    tx_buffer.len = frame_len;
    tx_buffer.next = RT_NULL;

    TxConfig.Length = frame_len;
    TxConfig.TxBuffer = &tx_buffer;
    TxConfig.pData = RT_NULL;

    if (HAL_ETH_Transmit(&heth1, &TxConfig, 100U) != HAL_OK)
    {
        result = -RT_ERROR;
    }

__exit:
#if ETH_PAD_SIZE
    (void)pbuf_add_header(p, ETH_PAD_SIZE);
#endif

    return result;
}

static struct pbuf *bsp_eth_rx(rt_device_t dev)
{
    struct pbuf *p = RT_NULL;

    RT_UNUSED(dev);

    if (eth_rx_alloc_status != BSP_ETH_RX_ALLOC_OK)
    {
        return RT_NULL;
    }
    if (HAL_ETH_ReadData(&heth1, (void **)&p) != HAL_OK)
    {
        return RT_NULL;
    }

    return p;
}

int BSP_ETH_Init(void)
{
    memset(&heth1, 0, sizeof(heth1));

    heth1.Instance = ETH;
    heth1.Init.MACAddr = eth_mac_addr;
    heth1.Init.MediaInterface = HAL_ETH_RGMII_MODE;
    heth1.Init.TxDesc = eth_tx_desc;
    heth1.Init.RxDesc = eth_rx_desc;
    heth1.Init.RxBuffLen = BSP_ETH_RX_BUFFER_SIZE;

    RCC->MP_AHB6ENSETR = RCC_MP_AHB6ENSETR_ETH1MACEN |
                         RCC_MP_AHB6ENSETR_ETH1RXEN |
                         RCC_MP_AHB6ENSETR_ETH1TXEN |
                         RCC_MP_AHB6ENSETR_ETH1CKEN;

    if (HAL_ETH_Init(&heth1) != HAL_OK)
    {
        return -RT_ERROR;
    }

    memset(&TxConfig, 0, sizeof(TxConfig));
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    return (bsp_eth_phy_init() == RT_EOK) ? 0 : -RT_ERROR;
}

static int rt_hw_eth_init(void)
{
    rt_err_t result;

    LWIP_MEMPOOL_INIT(ETH_RX_POOL);

    if (BSP_ETH_Init() != 0)
    {
        rt_kprintf("eth hardware init failed\n");
        return -RT_ERROR;
    }

    memset(&eth_device, 0, sizeof(eth_device));
#ifdef RT_USING_DEVICE_OPS
    eth_device.parent.ops = &bsp_eth_ops;
#else
    eth_device.parent.init = bsp_eth_dev_init;
    eth_device.parent.open = bsp_eth_dev_open;
    eth_device.parent.close = bsp_eth_dev_close;
    eth_device.parent.read = bsp_eth_dev_read;
    eth_device.parent.write = bsp_eth_dev_write;
    eth_device.parent.control = bsp_eth_dev_control;
#endif
    eth_device.eth_rx = bsp_eth_rx;
    eth_device.eth_tx = bsp_eth_tx;

    result = eth_device_init(&eth_device, BSP_ETH_DEVICE_NAME);
    if (result != RT_EOK)
    {
        rt_kprintf("eth device register failed: %d\n", result);
        return result;
    }

    eth_link_thread = rt_thread_create("ethlink",
                                       bsp_eth_link_thread_entry,
                                       RT_NULL,
                                       1024,
                                       RT_LWIP_ETHTHREAD_PRIORITY,
                                       20);
    if (eth_link_thread != RT_NULL)
    {
        rt_thread_startup(eth_link_thread);
    }
    else
    {
        rt_kprintf("eth link thread create failed\n");
    }

    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_hw_eth_init);

void HAL_ETH_MspInit(ETH_HandleTypeDef *eth_handle)
{
    GPIO_InitTypeDef gpio_init = {0};
    RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

    if (eth_handle->Instance != ETH)
    {
        return;
    }

    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ETH1;
    periph_clk_init.Eth1ClockSelection = RCC_ETH1CLKSOURCE_PLL4;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_ETH1CK_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_11;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    gpio_init.Pin = GPIO_PIN_2;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_2;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOG, &gpio_init);

    gpio_init.Pin = GPIO_PIN_1 | GPIO_PIN_7;
    gpio_init.Mode = GPIO_MODE_AF;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = GPIO_PIN_5 | GPIO_PIN_4;
    gpio_init.Mode = GPIO_MODE_AF;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    gpio_init.Pin = GPIO_PIN_12;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOF, &gpio_init);

    gpio_init.Pin = GPIO_PIN_1 | GPIO_PIN_0;
    gpio_init.Mode = GPIO_MODE_AF;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    gpio_init.Pin = GPIO_PIN_5;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF10_ETH;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    gpio_init.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    IRQ_Disable(ETH1_IRQn);
    IRQ_ClearPending(ETH1_IRQn);
    IRQ_SetHandler(ETH1_IRQn, bsp_eth_irq_handler);
    IRQ_SetPriority(ETH1_IRQn, BSP_ETH_IRQ_PRIORITY);
    IRQ_SetMode(ETH1_IRQn, IRQ_MODE_TRIG_LEVEL | IRQ_MODE_TYPE_IRQ | IRQ_MODE_CPU_0);
    IRQ_Enable(ETH1_IRQn);
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef *eth_handle)
{
    if (eth_handle->Instance != ETH)
    {
        return;
    }

    IRQ_Disable(ETH1_IRQn);
    IRQ_ClearPending(ETH1_IRQn);

    __HAL_RCC_ETH1CK_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11 | GPIO_PIN_1 | GPIO_PIN_0);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_7);
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_2);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_1 | GPIO_PIN_2);
    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_5);
}

void HAL_ETH_RxAllocateCallback(ETH_HandleTypeDef *heth, uint8_t **buff)
{
    struct pbuf_custom *custom_pbuf;
    rt_uint8_t *buffer;

    RT_UNUSED(heth);

    custom_pbuf = LWIP_MEMPOOL_ALLOC(ETH_RX_POOL);
    if (custom_pbuf == RT_NULL)
    {
        eth_rx_alloc_status = BSP_ETH_RX_ALLOC_ERROR;
        *buff = RT_NULL;
        return;
    }

    buffer = (rt_uint8_t *)custom_pbuf + offsetof(bsp_eth_rx_buff_t, buff);
    *buff = buffer + ETH_PAD_SIZE;

    custom_pbuf->custom_free_function = bsp_eth_pbuf_free_custom;
    (void)pbuf_alloced_custom(PBUF_RAW,
                              0,
                              PBUF_REF,
                              custom_pbuf,
                              buffer,
                              BSP_ETH_RX_BUFFER_SIZE - ETH_PAD_SIZE);
}

void HAL_ETH_RxLinkCallback(ETH_HandleTypeDef *heth,
                            void **p_start,
                            void **p_end,
                            uint8_t *buff,
                            uint16_t length)
{
    struct pbuf **pp_start = (struct pbuf **)p_start;
    struct pbuf **pp_end = (struct pbuf **)p_end;
    struct pbuf *p;

    RT_UNUSED(heth);

    if ((buff == RT_NULL) || (length == 0U))
    {
        return;
    }

    bsp_eth_cache_invalidate(buff, length);

    p = (struct pbuf *)(buff - ETH_PAD_SIZE - offsetof(bsp_eth_rx_buff_t, buff));
    p->next = RT_NULL;
    p->tot_len = length + ETH_PAD_SIZE;
    p->len = length + ETH_PAD_SIZE;

    if (*pp_start == RT_NULL)
    {
        *pp_start = p;
    }
    else
    {
        (*pp_end)->next = p;
    }

    *pp_end = p;
}

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    RT_UNUSED(heth);
    (void)eth_device_ready(&eth_device);
}

static void bsp_eth_pbuf_free_custom(struct pbuf *p)
{
    struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;

    LWIP_MEMPOOL_FREE(ETH_RX_POOL, custom_pbuf);

    if (eth_rx_alloc_status == BSP_ETH_RX_ALLOC_ERROR)
    {
        eth_rx_alloc_status = BSP_ETH_RX_ALLOC_OK;
        (void)eth_device_ready(&eth_device);
    }
}

static void bsp_eth_irq_handler(void)
{
    HAL_ETH_IRQHandler(&heth1);
}

#endif
