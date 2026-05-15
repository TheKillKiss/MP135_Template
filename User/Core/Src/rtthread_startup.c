#include "board.h"
#include <rthw.h>
#include <rtthread.h>

#ifdef RT_USING_FINSH
extern int finsh_system_init(void);
#endif

rt_weak void rt_user_main_entry(void)
{
    while (1)
    {
        rt_thread_mdelay(1000);
    }
}

static struct rt_thread main_thread;
rt_align(RT_ALIGN_SIZE)
static rt_uint8_t main_thread_stack[RT_MAIN_THREAD_STACK_SIZE];

static void main_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);
    rt_user_main_entry();
}

void rt_application_init(void)
{
    rt_err_t result;

    result = rt_thread_init(&main_thread,
                            "main",
                            main_thread_entry,
                            RT_NULL,
                            main_thread_stack,
                            sizeof(main_thread_stack),
                            RT_MAIN_THREAD_PRIORITY,
                            20);
    RT_ASSERT(result == RT_EOK);

    rt_thread_startup(&main_thread);
}

int rtthread_startup(void)
{
    rt_hw_interrupt_disable();

    rt_hw_board_init();
    rt_show_version();
    rt_system_timer_init();
    rt_system_scheduler_init();
    rt_application_init();
    rt_system_timer_thread_init();
    rt_thread_idle_init();
    rt_thread_defunct_init();
#ifdef RT_USING_FINSH
    finsh_system_init();
#endif
    rt_system_scheduler_start();

    return 0;
}
