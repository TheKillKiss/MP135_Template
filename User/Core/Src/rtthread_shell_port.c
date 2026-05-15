#include "board.h"
#include "irq_ctrl.h"
#include "main.h"
#include <rthw.h>
#include <rtdevice.h>
#include <rtthread.h>

UART_HandleTypeDef huart4;

static struct rt_serial_device uart4_serial;

static rt_err_t uart4_configure(struct rt_serial_device *serial,
                                struct serial_configure *cfg);
static rt_err_t uart4_control(struct rt_serial_device *serial, int cmd, void *arg);
static int uart4_putc(struct rt_serial_device *serial, char c);
static int uart4_getc(struct rt_serial_device *serial);
static void uart4_irq_handler(void);

static const struct rt_uart_ops uart4_ops =
{
    uart4_configure,
    uart4_control,
    uart4_putc,
    uart4_getc,
    RT_NULL
};

void rt_hw_uart4_console_init(void)
{
    rt_err_t result;

    uart4_serial.ops = &uart4_ops;
    uart4_serial.config = (struct serial_configure)RT_SERIAL_CONFIG_DEFAULT;
    uart4_serial.config.bufsz = RT_SERIAL_RB_BUFSZ;

    result = rt_hw_serial_register(&uart4_serial,
                                   RT_CONSOLE_DEVICE_NAME,
                                   RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                                   RT_NULL);
    RT_ASSERT(result == RT_EOK);

    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
}

void HAL_UART_MspInit(UART_HandleTypeDef *uart_handle)
{
    GPIO_InitTypeDef gpio_init = {0};
    RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

    if (uart_handle->Instance != UART4)
    {
        return;
    }

    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_UART4;
    periph_clk_init.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_UART4_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_8;
    gpio_init.Mode = GPIO_MODE_AF;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOD, &gpio_init);

    gpio_init.Pin = GPIO_PIN_6;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOD, &gpio_init);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uart_handle)
{
    if (uart_handle->Instance != UART4)
    {
        return;
    }

    __HAL_RCC_UART4_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8 | GPIO_PIN_6);
}

static rt_err_t uart4_configure(struct rt_serial_device *serial,
                                struct serial_configure *cfg)
{
    RT_UNUSED(serial);

    huart4.Instance = UART4;
    huart4.Init.BaudRate = cfg->baud_rate;

    switch (cfg->data_bits)
    {
    case DATA_BITS_7:
        huart4.Init.WordLength = UART_WORDLENGTH_7B;
        break;
    case DATA_BITS_8:
        huart4.Init.WordLength = UART_WORDLENGTH_8B;
        break;
    case DATA_BITS_9:
        huart4.Init.WordLength = UART_WORDLENGTH_9B;
        break;
    default:
        return -RT_EINVAL;
    }

    switch (cfg->stop_bits)
    {
    case STOP_BITS_1:
        huart4.Init.StopBits = UART_STOPBITS_1;
        break;
    case STOP_BITS_2:
        huart4.Init.StopBits = UART_STOPBITS_2;
        break;
    default:
        return -RT_EINVAL;
    }

    switch (cfg->parity)
    {
    case PARITY_NONE:
        huart4.Init.Parity = UART_PARITY_NONE;
        break;
    case PARITY_ODD:
        huart4.Init.Parity = UART_PARITY_ODD;
        break;
    case PARITY_EVEN:
        huart4.Init.Parity = UART_PARITY_EVEN;
        break;
    default:
        return -RT_EINVAL;
    }

    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
        return -RT_ERROR;
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        return -RT_ERROR;
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        return -RT_ERROR;
    }
    if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t uart4_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    rt_ubase_t flag = (rt_ubase_t)arg;

    RT_UNUSED(serial);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_SET_INT:
        if (flag & RT_DEVICE_FLAG_INT_RX)
        {
            __HAL_UART_CLEAR_PEFLAG(&huart4);
            __HAL_UART_CLEAR_FEFLAG(&huart4);
            __HAL_UART_CLEAR_NEFLAG(&huart4);
            __HAL_UART_CLEAR_OREFLAG(&huart4);
            __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
            __HAL_UART_ENABLE_IT(&huart4, UART_IT_ERR);
            __HAL_UART_ENABLE_IT(&huart4, UART_IT_PE);

            IRQ_Disable(UART4_IRQn);
            IRQ_ClearPending(UART4_IRQn);
            IRQ_SetHandler(UART4_IRQn, uart4_irq_handler);
            IRQ_SetPriority(UART4_IRQn, RT_UART4_IRQ_PRIORITY);
            IRQ_SetMode(UART4_IRQn, IRQ_MODE_TRIG_LEVEL);
            IRQ_Enable(UART4_IRQn);
        }
        break;

    case RT_DEVICE_CTRL_CLR_INT:
        if (flag & RT_DEVICE_FLAG_INT_RX)
        {
            __HAL_UART_DISABLE_IT(&huart4, UART_IT_RXNE);
            __HAL_UART_DISABLE_IT(&huart4, UART_IT_ERR);
            __HAL_UART_DISABLE_IT(&huart4, UART_IT_PE);
            IRQ_Disable(UART4_IRQn);
            IRQ_ClearPending(UART4_IRQn);
        }
        break;

    case RT_DEVICE_CTRL_CLOSE:
        __HAL_UART_DISABLE_IT(&huart4, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&huart4, UART_IT_ERR);
        __HAL_UART_DISABLE_IT(&huart4, UART_IT_PE);
        IRQ_Disable(UART4_IRQn);
        break;

    default:
        break;
    }

    return RT_EOK;
}

static int uart4_putc(struct rt_serial_device *serial, char c)
{
    RT_UNUSED(serial);

    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TXE) == RESET)
    {
    }

    huart4.Instance->TDR = (rt_uint8_t)c;
    return 1;
}

static int uart4_getc(struct rt_serial_device *serial)
{
    RT_UNUSED(serial);

    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE) == RESET)
    {
        return -1;
    }

    return (int)(huart4.Instance->RDR & 0xffU);
}

static void uart4_irq_handler(void)
{
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart4);
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(&huart4);
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(&huart4);
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart4);
    }

    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE) != RESET)
    {
        rt_hw_serial_isr(&uart4_serial, RT_SERIAL_EVENT_RX_IND);
    }
}
