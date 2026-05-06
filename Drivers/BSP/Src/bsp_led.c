#include "bsp_led.h"
#include "os.h"

static OS_TCB  LedTaskTCB;
static CPU_STK LedTaskStk[LED_TASK_STK_SIZE];

static void BSP_LED_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOI_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = LED_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

void BSP_LED_On(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

void BSP_LED_Off(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
}

void BSP_LED_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
}

static void LedTask(void *p_arg)
{
    OS_ERR err;

    (void)p_arg;

    while (1) {
        BSP_LED_Toggle();

        OSTimeDlyHMSM(0u, 0u, 0u, 500u,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
    }
}


void BSP_LED_Init(void)
{
    OS_ERR err;

    BSP_LED_GPIO_Init();
    BSP_LED_Off();

    
    OSTaskCreate((OS_TCB     *)&LedTaskTCB,
                 (CPU_CHAR   *)"LED Task",
                 (OS_TASK_PTR ) LedTask,
                 (void       *) 0,
                 (OS_PRIO     ) LED_TASK_PRIO,
                 (CPU_STK    *)&LedTaskStk[0],
                 (CPU_STK_SIZE) LED_TASK_STK_SIZE / 10u,
                 (CPU_STK_SIZE) LED_TASK_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

    if (err != OS_ERR_NONE) {
        Error_Handler();
    }
}

