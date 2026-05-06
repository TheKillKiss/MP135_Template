#ifndef BSP_LED_H
#define BSP_LED_H

#include "main.h"

#define LED_GPIO_PORT GPIOI
#define LED_GPIO_PIN  GPIO_PIN_3

#define LED_TASK_PRIO      0u
#define LED_TASK_STK_SIZE  512u

void BSP_LED_Init(void);
void BSP_LED_On(void);
void BSP_LED_Off(void);
void BSP_LED_Toggle(void);

#endif /* BSP_LED_H */
