#include "task_1ms.h"
#include "os.h"

static OS_TCB  Sys1msTaskTCB;
static CPU_STK Sys1msTaskStk[SYS1MS_TASK_STK_SIZE];

static void Sys1msTask(void *p_arg)
{
    OS_ERR err;

    (void)p_arg;

    

    while (1) {

        OSTimeDly(1, OS_OPT_TIME_DLY, &err);
    }
}

void Sys1ms_Task_Init(void)
{
    OS_ERR err;
    
    OSTaskCreate((OS_TCB     *)&Sys1msTaskTCB,
                 (CPU_CHAR   *)"1ms Task",
                 (OS_TASK_PTR ) Sys1msTask,
                 (void       *) 0,
                 (OS_PRIO     ) SYS1MS_TASK_PRIO,
                 (CPU_STK    *)&Sys1msTaskStk[0],
                 (CPU_STK_SIZE) SYS1MS_TASK_STK_SIZE / 10u,
                 (CPU_STK_SIZE) SYS1MS_TASK_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

    if (err != OS_ERR_NONE) {
        Error_Handler();
    }
}
