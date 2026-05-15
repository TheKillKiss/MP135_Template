#include "main.h"
#include "bsp_led.h"
#include <rtthread.h>

void rt_user_main_entry(void)
{
    while (1)
    {
        BSP_LED_Toggle();
        rt_thread_mdelay(500);
    }
}

int main(void)
{
    rt_user_main_entry();
    return 0;
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
