#ifndef BOARD_H__
#define BOARD_H__

#include "stm32mp13xx_hal.h"

#define RT_TICK_IRQ_PRIORITY           (0xA0U)
#define RT_UART4_IRQ_PRIORITY          (0xA0U)

void rt_hw_board_init(void);
void rt_hw_tick_init(void);

#endif
