#include "main.h"
#include "bsp_led.h"

int main()
{       
    HAL_Init();
    BSP_LED_Init();

    while(1){
        BSP_LED_Toggle();
        HAL_Delay(300);
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
