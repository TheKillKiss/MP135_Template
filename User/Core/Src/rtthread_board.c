#include "board.h"
#include "bsp_led.h"
#include "irq_ctrl.h"
#include <rthw.h>
#include <rtthread.h>

extern void _thread_start(void);

#define RT_GIC_HIGHEST_INTERRUPT_VALUE 1020U
#define RT_GIC_ACKNOWLEDGE_RESPONSE    1023U

rt_uint32_t rt_interrupt_from_thread;
rt_uint32_t rt_interrupt_to_thread;
rt_uint32_t rt_thread_switch_interrupt_flag;

static rt_uint32_t rt_hw_timer_frequency(void)
{
    if ((RCC->STGENCKSELR & RCC_STGENCKSELR_STGENSRC) == RCC_STGENCLKSOURCE_HSE)
    {
        return HSE_VALUE;
    }

    return HSI_VALUE;
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RT_UNUSED(TickPriority);
    return HAL_OK;
}

static void rt_hw_tick_handler(void)
{
    rt_uint32_t reload = rt_hw_timer_frequency() / RT_TICK_PER_SECOND;

    IRQ_ClearPending(SecurePhyTimer_IRQn);
    PL1_SetLoadValue(reload + PL1_GetCurrentValue());
    rt_tick_increase();
}

void rt_hw_tick_init(void)
{
    rt_uint32_t reload = rt_hw_timer_frequency() / RT_TICK_PER_SECOND;

    PL1_SetControl(0x0U);
    PL1_SetCounterFrequency(rt_hw_timer_frequency());
    PL1_SetLoadValue(reload);

    IRQ_Disable(SecurePhyTimer_IRQn);
    IRQ_ClearPending(SecurePhyTimer_IRQn);
    IRQ_SetHandler(SecurePhyTimer_IRQn, rt_hw_tick_handler);
    IRQ_SetPriority(SecurePhyTimer_IRQn, RT_TICK_IRQ_PRIORITY);
    IRQ_SetMode(SecurePhyTimer_IRQn, IRQ_MODE_TRIG_EDGE);
    IRQ_Enable(SecurePhyTimer_IRQn);

    PL1_SetControl(0x1U);
}

void rt_hw_board_init(void)
{
    HAL_Init();
    BSP_LED_Init();
    rt_hw_tick_init();
}

void rt_hw_trap_irq(void)
{
    while (1)
    {
        IRQn_ID_t irq = IRQ_GetActiveIRQ();

        if (irq <= RT_GIC_HIGHEST_INTERRUPT_VALUE)
        {
            IRQHandler_t handler = IRQ_GetHandler(irq);

            if (handler != RT_NULL)
            {
                handler();
            }

            IRQ_EndOfInterrupt(irq);
        }
        else if (irq == RT_GIC_ACKNOWLEDGE_RESPONSE)
        {
            break;
        }
        else
        {
            break;
        }
    }
}

rt_uint8_t *rt_hw_stack_init(void *tentry, void *parameter,
                             rt_uint8_t *stack_addr, void *texit)
{
    rt_uint32_t *stk;

    stack_addr += sizeof(rt_uint32_t);
    stack_addr = (rt_uint8_t *)RT_ALIGN_DOWN((rt_uint32_t)stack_addr, 8);
    stk = (rt_uint32_t *)stack_addr;

    *(--stk) = (rt_uint32_t)_thread_start;
    *(--stk) = (rt_uint32_t)texit;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = 0xdeadbeefU;
    *(--stk) = (rt_uint32_t)tentry;
    *(--stk) = (rt_uint32_t)parameter;

    /*
     * The initial PC is _thread_start, which is ARM code in rtthread_port_iar.s.
     * It uses BLX to enter the real thread entry, so the entry address itself
     * may carry the Thumb bit while the initial CPSR must stay in ARM state.
     */
    *(--stk) = 0x13U;

    return (rt_uint8_t *)stk;
}

rt_bool_t rt_hw_interrupt_is_disabled(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    rt_bool_t disabled = (level & 0x80U) ? RT_TRUE : RT_FALSE;

    rt_hw_interrupt_enable(level);
    return disabled;
}

void rt_hw_cpu_shutdown(void)
{
    rt_hw_interrupt_disable();
    while (1)
    {
    }
}
