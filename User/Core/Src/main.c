#include "main.h"
#include "bsp_led.h"

void SystemClock_Config(void);

int main()
{       
    HAL_Init();
    SystemClock_Config();
    BSP_LED_Init();

    while(1){
        BSP_LED_Toggle();
        HAL_Delay(300);
    }
}

void SystemClock_Config(void)
{
    HAL_RCC_DeInit();
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = 16;
    RCC_OscInitStruct.HSIDivValue = RCC_HSI_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLL12SOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 2;
    RCC_OscInitStruct.PLL.PLLN = 83;
    RCC_OscInitStruct.PLL.PLLP = 1;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLFRACV = 2730;
    RCC_OscInitStruct.PLL.PLLMODE = RCC_PLL_FRACTIONAL;
    RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL2.PLLSource = RCC_PLL12SOURCE_HSE;
    RCC_OscInitStruct.PLL2.PLLM = 2;
    RCC_OscInitStruct.PLL2.PLLN = 40;
    RCC_OscInitStruct.PLL2.PLLP = 2;
    RCC_OscInitStruct.PLL2.PLLQ = 2;
    RCC_OscInitStruct.PLL2.PLLR = 2;
    RCC_OscInitStruct.PLL2.PLLFRACV = 0;
    RCC_OscInitStruct.PLL2.PLLMODE = RCC_PLL_INTEGER;
    RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL3.PLLSource = RCC_PLL3SOURCE_HSI;
    RCC_OscInitStruct.PLL3.PLLM = 4;
    RCC_OscInitStruct.PLL3.PLLN = 25;
    RCC_OscInitStruct.PLL3.PLLP = 2;
    RCC_OscInitStruct.PLL3.PLLQ = 2;
    RCC_OscInitStruct.PLL3.PLLR = 2;
    RCC_OscInitStruct.PLL3.PLLRGE = RCC_PLL3IFRANGE_1;
    RCC_OscInitStruct.PLL3.PLLFRACV = 0;
    RCC_OscInitStruct.PLL3.PLLMODE = RCC_PLL_INTEGER;
    RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** RCC Clock Config
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_ACLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_PCLK3|RCC_CLOCKTYPE_PCLK4
                                |RCC_CLOCKTYPE_PCLK5|RCC_CLOCKTYPE_MPU
                                |RCC_CLOCKTYPE_PCLK6;
    RCC_ClkInitStruct.MPUInit.MPU_Clock = RCC_MPUSOURCE_PLL1;
    RCC_ClkInitStruct.MPUInit.MPU_Div = RCC_MPU_DIV2;
    RCC_ClkInitStruct.AXISSInit.AXI_Clock = RCC_AXISSOURCE_PLL2;
    RCC_ClkInitStruct.AXISSInit.AXI_Div = RCC_AXI_DIV1;
    RCC_ClkInitStruct.MLAHBInit.MLAHB_Clock = RCC_MLAHBSSOURCE_PLL3;
    RCC_ClkInitStruct.MLAHBInit.MLAHB_Div = RCC_MLAHB_DIV1;
    RCC_ClkInitStruct.APB4_Div = RCC_APB4_DIV2;
    RCC_ClkInitStruct.APB5_Div = RCC_APB5_DIV4;
    RCC_ClkInitStruct.APB1_Div = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2_Div = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB3_Div = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB6_Div = RCC_APB6_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_STGEN;
    PeriphClkInitStruct.StgenClockSelection = RCC_STGENCLKSOURCE_HSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_STGENRO_CLK_ENABLE();
    __HAL_RCC_STGEN_CLK_ENABLE();

    STGENC->CNTFID0 = HSE_VALUE;
    STGENC->CNTCR = 1U;

    PL1_SetCounterFrequency(HSE_VALUE);
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
