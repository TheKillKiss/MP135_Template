#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/def.h"
#include "lwip/mem.h"

#include "os.h"
#include "cpu.h"

#include "main.h"

/*
 * This sys_arch.c is for uC/OS-III.
 *
 * Notes:
 * 1. Your system tick must call OSTimeTick().
 * 2. sys_now() depends on OSTimeGet().
 * 3. OS_CFG_TICK_RATE_HZ must be correctly configured.
 */

#ifndef LWIP_SYS_MAX_SEM
#define LWIP_SYS_MAX_SEM       32u
#endif

#ifndef LWIP_SYS_MAX_MUTEX
#define LWIP_SYS_MAX_MUTEX     16u
#endif

#ifndef LWIP_SYS_MAX_MBOX
#define LWIP_SYS_MAX_MBOX      16u
#endif

#ifndef LWIP_SYS_MAX_TASKS
#define LWIP_SYS_MAX_TASKS     8u
#endif

#ifndef LWIP_SYS_DEFAULT_STACK_SIZE
#define LWIP_SYS_DEFAULT_STACK_SIZE   1024u
#endif

#ifndef LWIP_SYS_THREAD_STACK_MIN
#define LWIP_SYS_THREAD_STACK_MIN     256u
#endif

#define LWIP_SYS_MBOX_EMPTY           0xFFFFFFFFu

typedef struct {
    OS_SEM  sem;
    CPU_BOOLEAN used;
} LWIP_UCOS_SEM;

typedef struct {
    OS_MUTEX mutex;
    CPU_BOOLEAN used;
} LWIP_UCOS_MUTEX;

typedef struct {
    OS_SEM sem;
    OS_MUTEX lock;
    CPU_BOOLEAN used;
} LWIP_UCOS_MBOX_CTRL;

typedef struct {
    OS_TCB tcb;
    CPU_STK *stk;
    CPU_STK_SIZE stk_size;
    CPU_BOOLEAN used;
} LWIP_UCOS_TASK;

static LWIP_UCOS_SEM       g_lwip_sem_pool[LWIP_SYS_MAX_SEM];
static LWIP_UCOS_MUTEX     g_lwip_mutex_pool[LWIP_SYS_MAX_MUTEX];
static LWIP_UCOS_MBOX_CTRL g_lwip_mbox_pool[LWIP_SYS_MAX_MBOX];
static LWIP_UCOS_TASK      g_lwip_task_pool[LWIP_SYS_MAX_TASKS];

static sys_sem_t g_netconn_sem;

/* ------------------------------------------------------------
 * Time convert helpers
 * ------------------------------------------------------------ */

static OS_TICK sys_ms_to_ticks(uint32_t ms)
{
    uint64_t ticks;

    if (ms == 0u) {
        return 0u;
    }

    ticks = ((uint64_t)ms * (uint64_t)OS_CFG_TICK_RATE_HZ + 999u) / 1000u;

    if (ticks == 0u) {
        ticks = 1u;
    }

    return (OS_TICK)ticks;
}

static uint32_t sys_ticks_to_ms(OS_TICK ticks)
{
    uint64_t ms;

    ms = ((uint64_t)ticks * 1000u) / (uint64_t)OS_CFG_TICK_RATE_HZ;

    return (uint32_t)ms;
}

/* ------------------------------------------------------------
 * Pool alloc helpers
 * ------------------------------------------------------------ */

static OS_SEM *sys_alloc_sem(void)
{
    CPU_SR_ALLOC();
    uint32_t i;

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_SEM; i++) {
        if (g_lwip_sem_pool[i].used == DEF_FALSE) {
            g_lwip_sem_pool[i].used = DEF_TRUE;
            CPU_CRITICAL_EXIT();
            return &g_lwip_sem_pool[i].sem;
        }
    }

    CPU_CRITICAL_EXIT();
    return NULL;
}

static void sys_free_sem_obj(OS_SEM *sem)
{
    CPU_SR_ALLOC();
    uint32_t i;

    if (sem == NULL) {
        return;
    }

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_SEM; i++) {
        if (&g_lwip_sem_pool[i].sem == sem) {
            g_lwip_sem_pool[i].used = DEF_FALSE;
            break;
        }
    }

    CPU_CRITICAL_EXIT();
}

static OS_MUTEX *sys_alloc_mutex(void)
{
    CPU_SR_ALLOC();
    uint32_t i;

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_MUTEX; i++) {
        if (g_lwip_mutex_pool[i].used == DEF_FALSE) {
            g_lwip_mutex_pool[i].used = DEF_TRUE;
            CPU_CRITICAL_EXIT();
            return &g_lwip_mutex_pool[i].mutex;
        }
    }

    CPU_CRITICAL_EXIT();
    return NULL;
}

static void sys_free_mutex_obj(OS_MUTEX *mutex)
{
    CPU_SR_ALLOC();
    uint32_t i;

    if (mutex == NULL) {
        return;
    }

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_MUTEX; i++) {
        if (&g_lwip_mutex_pool[i].mutex == mutex) {
            g_lwip_mutex_pool[i].used = DEF_FALSE;
            break;
        }
    }

    CPU_CRITICAL_EXIT();
}

static LWIP_UCOS_MBOX_CTRL *sys_alloc_mbox_ctrl(void)
{
    CPU_SR_ALLOC();
    uint32_t i;

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_MBOX; i++) {
        if (g_lwip_mbox_pool[i].used == DEF_FALSE) {
            g_lwip_mbox_pool[i].used = DEF_TRUE;
            CPU_CRITICAL_EXIT();
            return &g_lwip_mbox_pool[i];
        }
    }

    CPU_CRITICAL_EXIT();
    return NULL;
}

static void sys_free_mbox_ctrl(LWIP_UCOS_MBOX_CTRL *ctrl)
{
    CPU_SR_ALLOC();
    uint32_t i;

    if (ctrl == NULL) {
        return;
    }

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_MBOX; i++) {
        if (&g_lwip_mbox_pool[i] == ctrl) {
            g_lwip_mbox_pool[i].used = DEF_FALSE;
            break;
        }
    }

    CPU_CRITICAL_EXIT();
}

/* ------------------------------------------------------------
 * sys_init
 * ------------------------------------------------------------ */

void sys_init(void)
{
    uint32_t i;

    for (i = 0u; i < LWIP_SYS_MAX_SEM; i++) {
        g_lwip_sem_pool[i].used = DEF_FALSE;
    }

    for (i = 0u; i < LWIP_SYS_MAX_MUTEX; i++) {
        g_lwip_mutex_pool[i].used = DEF_FALSE;
    }

    for (i = 0u; i < LWIP_SYS_MAX_MBOX; i++) {
        g_lwip_mbox_pool[i].used = DEF_FALSE;
    }

    for (i = 0u; i < LWIP_SYS_MAX_TASKS; i++) {
        g_lwip_task_pool[i].used = DEF_FALSE;
        g_lwip_task_pool[i].stk = NULL;
        g_lwip_task_pool[i].stk_size = 0u;
    }

    sys_sem_set_invalid(&g_netconn_sem);
}

/* ------------------------------------------------------------
 * Semaphore
 * ------------------------------------------------------------ */

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    OS_ERR err;
    OS_SEM *os_sem;

    if (sem == NULL) {
        return ERR_ARG;
    }

    os_sem = sys_alloc_sem();
    if (os_sem == NULL) {
        sys_sem_set_invalid(sem);
        return ERR_MEM;
    }

    OSSemCreate(os_sem,
                "lwIP sem",
                (OS_SEM_CTR)count,
                &err);

    if (err != OS_ERR_NONE) {
        sys_free_sem_obj(os_sem);
        sys_sem_set_invalid(sem);
        return ERR_VAL;
    }

    sem->sem = os_sem;

    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    OS_ERR err;
    OS_SEM *os_sem;

    if (!sys_sem_valid(sem)) {
        return;
    }

    os_sem = (OS_SEM *)sem->sem;

    OSSemDel(os_sem,
             OS_OPT_DEL_ALWAYS,
             &err);

    sys_free_sem_obj(os_sem);
    sys_sem_set_invalid(sem);
}

void sys_sem_signal(sys_sem_t *sem)
{
    OS_ERR err;
    OS_SEM *os_sem;

    if (!sys_sem_valid(sem)) {
        return;
    }

    os_sem = (OS_SEM *)sem->sem;

    OSSemPost(os_sem,
              OS_OPT_POST_1,
              &err);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    OS_ERR err;
    OS_SEM *os_sem;
    OS_TICK start;
    OS_TICK end;
    OS_TICK wait_ticks;

    if (!sys_sem_valid(sem)) {
        return SYS_ARCH_TIMEOUT;
    }

    os_sem = (OS_SEM *)sem->sem;

    start = OSTimeGet(&err);

    if (timeout == 0u) {
        wait_ticks = 0u;          /* wait forever */
    } else {
        wait_ticks = sys_ms_to_ticks(timeout);
    }

    OSSemPend(os_sem,
              wait_ticks,
              OS_OPT_PEND_BLOCKING,
              NULL,
              &err);

    if (err == OS_ERR_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
    }

    if (err != OS_ERR_NONE) {
        return SYS_ARCH_TIMEOUT;
    }

    end = OSTimeGet(&err);

    return sys_ticks_to_ms(end - start);
}

/* ------------------------------------------------------------
 * Mutex
 * ------------------------------------------------------------ */

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    OS_ERR err;
    OS_MUTEX *os_mutex;

    if (mutex == NULL) {
        return ERR_ARG;
    }

    os_mutex = sys_alloc_mutex();
    if (os_mutex == NULL) {
        sys_mutex_set_invalid(mutex);
        return ERR_MEM;
    }

    OSMutexCreate(os_mutex,
                  "lwIP mutex",
                  &err);

    if (err != OS_ERR_NONE) {
        sys_free_mutex_obj(os_mutex);
        sys_mutex_set_invalid(mutex);
        return ERR_VAL;
    }

    mutex->mut = os_mutex;

    return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    OS_ERR err;
    OS_MUTEX *os_mutex;

    if (!sys_mutex_valid(mutex)) {
        return;
    }

    os_mutex = (OS_MUTEX *)mutex->mut;

    OSMutexDel(os_mutex,
               OS_OPT_DEL_ALWAYS,
               &err);

    sys_free_mutex_obj(os_mutex);
    sys_mutex_set_invalid(mutex);
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    OS_ERR err;
    OS_MUTEX *os_mutex;

    if (!sys_mutex_valid(mutex)) {
        return;
    }

    os_mutex = (OS_MUTEX *)mutex->mut;

    OSMutexPend(os_mutex,
                0u,
                OS_OPT_PEND_BLOCKING,
                NULL,
                &err);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    OS_ERR err;
    OS_MUTEX *os_mutex;

    if (!sys_mutex_valid(mutex)) {
        return;
    }

    os_mutex = (OS_MUTEX *)mutex->mut;

    OSMutexPost(os_mutex,
                OS_OPT_POST_NONE,
                &err);
}

/* ------------------------------------------------------------
 * Mailbox
 * ------------------------------------------------------------ */

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    OS_ERR err;
    LWIP_UCOS_MBOX_CTRL *ctrl;

    (void)size;

    if (mbox == NULL) {
        return ERR_ARG;
    }

    ctrl = sys_alloc_mbox_ctrl();
    if (ctrl == NULL) {
        sys_mbox_set_invalid(mbox);
        return ERR_MEM;
    }

    OSSemCreate(&ctrl->sem,
                "lwIP mbox sem",
                0u,
                &err);

    if (err != OS_ERR_NONE) {
        sys_free_mbox_ctrl(ctrl);
        sys_mbox_set_invalid(mbox);
        return ERR_VAL;
    }

    OSMutexCreate(&ctrl->lock,
                  "lwIP mbox lock",
                  &err);

    if (err != OS_ERR_NONE) {
        OSSemDel(&ctrl->sem, OS_OPT_DEL_ALWAYS, &err);
        sys_free_mbox_ctrl(ctrl);
        sys_mbox_set_invalid(mbox);
        return ERR_VAL;
    }

    mbox->sem = ctrl;
    mbox->head = 0u;
    mbox->tail = 0u;

    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    OS_ERR err;
    LWIP_UCOS_MBOX_CTRL *ctrl;

    if (!sys_mbox_valid(mbox)) {
        return;
    }

    ctrl = (LWIP_UCOS_MBOX_CTRL *)mbox->sem;

    OSSemDel(&ctrl->sem,
             OS_OPT_DEL_ALWAYS,
             &err);

    OSMutexDel(&ctrl->lock,
               OS_OPT_DEL_ALWAYS,
               &err);

    sys_free_mbox_ctrl(ctrl);
    sys_mbox_set_invalid(mbox);
}

static err_t sys_mbox_post_internal(sys_mbox_t *mbox, void *msg, CPU_BOOLEAN blocking)
{
    OS_ERR err;
    LWIP_UCOS_MBOX_CTRL *ctrl;
    uint32_t next;

    if (!sys_mbox_valid(mbox)) {
        return ERR_VAL;
    }

    ctrl = (LWIP_UCOS_MBOX_CTRL *)mbox->sem;

    while (1) {
        OSMutexPend(&ctrl->lock,
                    0u,
                    OS_OPT_PEND_BLOCKING,
                    NULL,
                    &err);

        if (err != OS_ERR_NONE) {
            return ERR_VAL;
        }

        next = (mbox->head + 1u) % MAX_QUEUE_ENTRIES;

        if (next != mbox->tail) {
            mbox->q_mem[mbox->head] = msg;
            mbox->head = next;

            OSMutexPost(&ctrl->lock,
                        OS_OPT_POST_NONE,
                        &err);

            OSSemPost(&ctrl->sem,
                      OS_OPT_POST_1,
                      &err);

            return ERR_OK;
        }

        OSMutexPost(&ctrl->lock,
                    OS_OPT_POST_NONE,
                    &err);

        if (blocking == DEF_FALSE) {
            return ERR_MEM;
        }

        /*
         * Mailbox is full.
         * uC/OS-III semaphore here counts messages, not free slots,
         * so we simply delay one tick and retry.
         */
        OSTimeDly(1u,
                  OS_OPT_TIME_DLY,
                  &err);
    }
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    (void)sys_mbox_post_internal(mbox, msg, DEF_TRUE);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_post_internal(mbox, msg, DEF_FALSE);
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    /*
     * This simple implementation does not support posting to mbox from ISR.
     * If your Ethernet RX ISR needs this, implement a real ISR-safe queue.
     */
    return sys_mbox_post_internal(mbox, msg, DEF_FALSE);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    OS_ERR err;
    LWIP_UCOS_MBOX_CTRL *ctrl;
    OS_TICK wait_ticks;
    OS_TICK start;
    OS_TICK end;
    void *local_msg;

    if (!sys_mbox_valid(mbox)) {
        return SYS_ARCH_TIMEOUT;
    }

    ctrl = (LWIP_UCOS_MBOX_CTRL *)mbox->sem;

    if (timeout == 0u) {
        wait_ticks = 0u;          /* wait forever */
    } else {
        wait_ticks = sys_ms_to_ticks(timeout);
    }

    start = OSTimeGet(&err);

    OSSemPend(&ctrl->sem,
              wait_ticks,
              OS_OPT_PEND_BLOCKING,
              NULL,
              &err);

    if (err == OS_ERR_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
    }

    if (err != OS_ERR_NONE) {
        return SYS_ARCH_TIMEOUT;
    }

    OSMutexPend(&ctrl->lock,
                0u,
                OS_OPT_PEND_BLOCKING,
                NULL,
                &err);

    if (err != OS_ERR_NONE) {
        return SYS_ARCH_TIMEOUT;
    }

    if (mbox->tail == mbox->head) {
        local_msg = NULL;
    } else {
        local_msg = mbox->q_mem[mbox->tail];
        mbox->tail = (mbox->tail + 1u) % MAX_QUEUE_ENTRIES;
    }

    OSMutexPost(&ctrl->lock,
                OS_OPT_POST_NONE,
                &err);

    if (msg != NULL) {
        *msg = local_msg;
    }

    end = OSTimeGet(&err);

    return sys_ticks_to_ms(end - start);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    OS_ERR err;
    LWIP_UCOS_MBOX_CTRL *ctrl;
    OS_SEM_CTR ctr;
    void *local_msg;

    if (!sys_mbox_valid(mbox)) {
        return SYS_MBOX_EMPTY;
    }

    ctrl = (LWIP_UCOS_MBOX_CTRL *)mbox->sem;

    ctr = OSSemPend(&ctrl->sem,
                    0u,
                    OS_OPT_PEND_NON_BLOCKING,
                    NULL,
                    &err);

    (void)ctr;

    if (err != OS_ERR_NONE) {
        return SYS_MBOX_EMPTY;
    }

    OSMutexPend(&ctrl->lock,
                0u,
                OS_OPT_PEND_BLOCKING,
                NULL,
                &err);

    if (err != OS_ERR_NONE) {
        return SYS_MBOX_EMPTY;
    }

    if (mbox->tail == mbox->head) {
        local_msg = NULL;
    } else {
        local_msg = mbox->q_mem[mbox->tail];
        mbox->tail = (mbox->tail + 1u) % MAX_QUEUE_ENTRIES;
    }

    OSMutexPost(&ctrl->lock,
                OS_OPT_POST_NONE,
                &err);

    if (msg != NULL) {
        *msg = local_msg;
    }

    return 0u;
}

/* ------------------------------------------------------------
 * Thread
 * ------------------------------------------------------------ */

typedef struct {
    lwip_thread_fn fn;
    void *arg;
} LWIP_THREAD_ARG;

static void sys_thread_entry(void *p_arg)
{
    LWIP_THREAD_ARG *thread_arg;

    thread_arg = (LWIP_THREAD_ARG *)p_arg;

    if (thread_arg != NULL) {
        thread_arg->fn(thread_arg->arg);
    }

    while (1) {
        OS_ERR err;
        OSTimeDly(1000u,
                  OS_OPT_TIME_DLY,
                  &err);
    }
}

sys_thread_t sys_thread_new(const char *name,
                            lwip_thread_fn thread,
                            void *arg,
                            int stacksize,
                            int prio)
{
    OS_ERR err;
    CPU_SR_ALLOC();
    uint32_t i;
    LWIP_UCOS_TASK *task = NULL;
    CPU_STK_SIZE stk_size;
    LWIP_THREAD_ARG *thread_arg;

    if (thread == NULL) {
        return 0u;
    }

    if (stacksize <= 0) {
        stk_size = LWIP_SYS_DEFAULT_STACK_SIZE;
    } else {
        stk_size = (CPU_STK_SIZE)stacksize;
    }

    if (stk_size < LWIP_SYS_THREAD_STACK_MIN) {
        stk_size = LWIP_SYS_THREAD_STACK_MIN;
    }

    CPU_CRITICAL_ENTER();

    for (i = 0u; i < LWIP_SYS_MAX_TASKS; i++) {
        if (g_lwip_task_pool[i].used == DEF_FALSE) {
            g_lwip_task_pool[i].used = DEF_TRUE;
            task = &g_lwip_task_pool[i];
            break;
        }
    }

    CPU_CRITICAL_EXIT();

    if (task == NULL) {
        return 0u;
    }

    task->stk = (CPU_STK *)mem_malloc(sizeof(CPU_STK) * stk_size);
    if (task->stk == NULL) {
        task->used = DEF_FALSE;
        return 0u;
    }

    task->stk_size = stk_size;

    thread_arg = (LWIP_THREAD_ARG *)mem_malloc(sizeof(LWIP_THREAD_ARG));
    if (thread_arg == NULL) {
        mem_free(task->stk);
        task->stk = NULL;
        task->used = DEF_FALSE;
        return 0u;
    }

    thread_arg->fn = thread;
    thread_arg->arg = arg;

    OSTaskCreate(&task->tcb,
                 (CPU_CHAR *)name,
                 sys_thread_entry,
                 thread_arg,
                 (OS_PRIO)prio,
                 &task->stk[0],
                 stk_size / 10u,
                 stk_size,
                 0u,
                 0u,
                 NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);

    if (err != OS_ERR_NONE) {
        mem_free(thread_arg);
        mem_free(task->stk);
        task->stk = NULL;
        task->used = DEF_FALSE;
        return 0u;
    }

    return (sys_thread_t)(i + 1u);
}

/* ------------------------------------------------------------
 * Time
 * ------------------------------------------------------------ */

u32_t sys_now(void)
{
    OS_ERR err;
    OS_TICK tick;

    tick = OSTimeGet(&err);

    return sys_ticks_to_ms(tick);
}

/* ------------------------------------------------------------
 * Critical section
 * ------------------------------------------------------------ */

sys_prot_t sys_arch_protect(void)
{
    CPU_SR cpu_sr;

    cpu_sr = 0u;
    CPU_CRITICAL_ENTER();

    return (sys_prot_t)cpu_sr;
}

/* ------------------------------------------------------------
 * netconn per-thread semaphore hooks
 * ------------------------------------------------------------ */

sys_sem_t *sys_arch_netconn_sem_get(void)
{
    return &g_netconn_sem;
}

void sys_arch_netconn_sem_alloc(void)
{
    if (!sys_sem_valid(&g_netconn_sem)) {
        (void)sys_sem_new(&g_netconn_sem, 0u);
    }
}

void sys_arch_netconn_sem_free(void)
{
    if (sys_sem_valid(&g_netconn_sem)) {
        sys_sem_free(&g_netconn_sem);
    }
}

/* ------------------------------------------------------------
 * TCP/IP core thread mark / lock
 * ------------------------------------------------------------ */

static OS_TCB *g_tcpip_thread_tcb = NULL;

void sys_mark_tcpip_thread(void)
{
    OS_ERR err;

    g_tcpip_thread_tcb = OSTaskQPend(0u,
                                     OS_OPT_PEND_NON_BLOCKING,
                                     NULL,
                                     NULL,
                                     &err);

    /*
     * 上面这种写法不适合所有 uC/OS-III 版本。
     * 如果你的 OS 没有办法这样取当前 TCB，可以直接留空。
     * lwIP 只在启用 LWIP_CHECK_CORE_LOCKING 时才严格依赖它。
     */
    (void)g_tcpip_thread_tcb;
}

#if LWIP_TCPIP_CORE_LOCKING

static sys_mutex_t g_tcpip_core_mutex;

void sys_lock_tcpip_core(void)
{
    if (!sys_mutex_valid(&g_tcpip_core_mutex)) {
        (void)sys_mutex_new(&g_tcpip_core_mutex);
    }

    sys_mutex_lock(&g_tcpip_core_mutex);
}

void sys_unlock_tcpip_core(void)
{
    sys_mutex_unlock(&g_tcpip_core_mutex);
}

#endif

int lwip_win32_keypressed(void)
{
    return 0;
}