#include "os_cpu.h"
#include "os.h"

extern void IRQ_Handler(void);
extern OS_TCB *OSTCBCurPtr;
volatile OS_CPU_ExceptInfo_t g_ExceptInfo;

static inline uint32_t ARM_Read_DFSR(void)
{
    uint32_t val;
    __asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r"(val));
    return val;
}

static inline uint32_t ARM_Read_IFSR(void)
{
    uint32_t val;
    __asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r"(val));
    return val;
}

static inline uint32_t ARM_Read_DFAR(void)
{
    uint32_t val;
    __asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r"(val));
    return val;
}

static inline uint32_t ARM_Read_IFAR(void)
{
    uint32_t val;
    __asm volatile ("mrc p15, 0, %0, c6, c0, 2" : "=r"(val));
    return val;
}

void HF_ERR(CPU_INT32U src_id)
{
    CPU_STK *p_stk;

    __asm volatile ("mov %0, sp" : "=r"(p_stk));

    g_ExceptInfo.src_id = src_id;

    g_ExceptInfo.spsr = p_stk[0];
    //g_ExceptInfo.lr   = p_stk[14];
    //g_ExceptInfo.pc   = p_stk[15];

    __asm volatile ("mrs %0, cpsr" : "=r"(g_ExceptInfo.cpsr));

    if (src_id == OS_CPU_ARM_EXCEPT_DATA_ABORT ||
        src_id == OS_CPU_ARM_EXCEPT_PREFETCH_ABORT ||
        src_id == OS_CPU_ARM_EXCEPT_UNDEF_INSTR)
    {
        g_ExceptInfo.dfsr = ARM_Read_DFSR();
        g_ExceptInfo.dfar = ARM_Read_DFAR();
        g_ExceptInfo.ifsr = ARM_Read_IFSR();
        g_ExceptInfo.ifar = ARM_Read_IFAR();
    }
}

void ddr_soft_reset(void)
{
    uint32_t addr = 0xC0000000;

    __asm volatile("cpsid i");

    __asm volatile(
        "mrc p15,0,r0,c1,c0,0\n"
        "bic r0,r0,#0x1\n"       /* MMU OFF */
        "bic r0,r0,#0x4\n"       /* DCache OFF */
        "bic r0,r0,#0x1000\n"    /* ICache OFF */
        "bic r0,r0,#0x800\n"     /* Branch predictor OFF */
        "mcr p15,0,r0,c1,c0,0\n"
        :::"r0"
    );

    __asm volatile(
        "mov r0,#0\n"
        "mcr p15,0,r0,c8,c7,0\n"
        :::"r0"
    );

    __asm volatile(
        "mcr p15,0,%0,c12,c0,0\n"
        :
        : "r"(addr)
    );

    __asm volatile(
        "dsb\n"
        "isb\n"
    );

    void (*entry)(void) = (void (*)(void))addr;
    entry();
}

void OS_CPU_ExceptHndlr(CPU_INT32U src_id, uint32_t fault_pc)
{
    g_ExceptInfo.pc = fault_pc;
    int exc_wait = 0;

    switch(src_id){
        case OS_CPU_ARM_EXCEPT_IRQ:  
            exc_wait = 0; 
            IRQ_Handler();              
            break;
        case OS_CPU_ARM_EXCEPT_FIQ:             exc_wait = 2;   break;
        case OS_CPU_ARM_EXCEPT_RESET:           exc_wait = 3;   break;
        case OS_CPU_ARM_EXCEPT_UNDEF_INSTR:     exc_wait = 4;   break;
        case OS_CPU_ARM_EXCEPT_SWI:             exc_wait = 5;   break;
        case OS_CPU_ARM_EXCEPT_PREFETCH_ABORT:  exc_wait = 6;   break;
        case OS_CPU_ARM_EXCEPT_DATA_ABORT:      exc_wait = 7;   break;
    }

    if(exc_wait != 0){
        HF_ERR(src_id);
        while(exc_wait){
            asm("nop");
            //ddr_soft_reset();
            HAL_GPIO_WritePin(GPIOI, GPIO_PIN_3, GPIO_PIN_RESET);
        }
        // HFProcess(exc_wait);
    }
}