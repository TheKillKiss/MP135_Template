#include "main.h"
#include "lwip.h"
#include "bsp_led.h"
#include "os.h"
#include "task_1ms.h"

int main(void)
{
    OS_ERR err;

    HAL_Init();

    OSInit(&err);
    if (err != OS_ERR_NONE) {
        Error_Handler();
    }

    BSP_LED_Init();
    LwIP_Init();
    Sys1ms_Task_Init();

    OSStart(&err);

    while (1) {

    }
}

void Error_Handler(void)
{

    __disable_irq();
    while (1)
    {
        BSP_LED_Toggle();
        HAL_Delay(100);
    }

}
