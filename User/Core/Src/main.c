#include "main.h"
#include "bsp_led.h"
#include <rtthread.h>

extern int rtthread_startup(void);

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
    rtthread_startup();

    while (1)
    {
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
