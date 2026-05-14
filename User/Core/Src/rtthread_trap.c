#include <rthw.h>
#include <rtthread.h>

struct rt_hw_exp_stack
{
    rt_uint32_t r0;
    rt_uint32_t r1;
    rt_uint32_t r2;
    rt_uint32_t r3;
    rt_uint32_t r4;
    rt_uint32_t r5;
    rt_uint32_t r6;
    rt_uint32_t r7;
    rt_uint32_t r8;
    rt_uint32_t r9;
    rt_uint32_t r10;
    rt_uint32_t fp;
    rt_uint32_t ip;
    rt_uint32_t sp;
    rt_uint32_t lr;
    rt_uint32_t pc;
    rt_uint32_t cpsr;
};

volatile struct rt_hw_exp_stack rt_hw_exception_stack;
volatile const char *rt_hw_exception_name;
volatile rt_uint32_t rt_hw_exception_fault_pc;
volatile rt_uint32_t rt_hw_exception_pc_adjust;

static void rt_hw_exception_save(const char *name,
                                 struct rt_hw_exp_stack *regs,
                                 rt_uint32_t pc_adjust)
{
    rt_hw_exception_stack.r0 = regs->r0;
    rt_hw_exception_stack.r1 = regs->r1;
    rt_hw_exception_stack.r2 = regs->r2;
    rt_hw_exception_stack.r3 = regs->r3;
    rt_hw_exception_stack.r4 = regs->r4;
    rt_hw_exception_stack.r5 = regs->r5;
    rt_hw_exception_stack.r6 = regs->r6;
    rt_hw_exception_stack.r7 = regs->r7;
    rt_hw_exception_stack.r8 = regs->r8;
    rt_hw_exception_stack.r9 = regs->r9;
    rt_hw_exception_stack.r10 = regs->r10;
    rt_hw_exception_stack.fp = regs->fp;
    rt_hw_exception_stack.ip = regs->ip;
    rt_hw_exception_stack.sp = regs->sp;
    rt_hw_exception_stack.lr = regs->lr;
    rt_hw_exception_stack.pc = regs->pc;
    rt_hw_exception_stack.cpsr = regs->cpsr;

    rt_hw_exception_name = name;
    rt_hw_exception_pc_adjust = pc_adjust;
    rt_hw_exception_fault_pc = regs->pc - pc_adjust;

    rt_kprintf("\n%s exception\n", name);
    rt_kprintf("fault pc: 0x%08x, exception lr/pc: 0x%08x, cpsr: 0x%08x\n",
               rt_hw_exception_fault_pc, regs->pc, regs->cpsr);
    rt_kprintf("r0 :0x%08x r1 :0x%08x r2 :0x%08x r3 :0x%08x\n",
               regs->r0, regs->r1, regs->r2, regs->r3);
    rt_kprintf("r4 :0x%08x r5 :0x%08x r6 :0x%08x r7 :0x%08x\n",
               regs->r4, regs->r5, regs->r6, regs->r7);
    rt_kprintf("r8 :0x%08x r9 :0x%08x r10:0x%08x fp :0x%08x\n",
               regs->r8, regs->r9, regs->r10, regs->fp);
    rt_kprintf("ip :0x%08x sp :0x%08x lr :0x%08x\n",
               regs->ip, regs->sp, regs->lr);

    rt_hw_cpu_shutdown();
}

void rt_hw_trap_undef(struct rt_hw_exp_stack *regs)
{
    rt_hw_exception_save("undefined instruction", regs, 4U);
}

void rt_hw_trap_swi(struct rt_hw_exp_stack *regs)
{
    rt_hw_exception_save("software interrupt", regs, 4U);
}

void rt_hw_trap_pabt(struct rt_hw_exp_stack *regs)
{
    rt_hw_exception_save("prefetch abort", regs, 4U);
}

void rt_hw_trap_dabt(struct rt_hw_exp_stack *regs)
{
    rt_hw_exception_save("data abort", regs, 8U);
}

void rt_hw_trap_resv(struct rt_hw_exp_stack *regs)
{
    rt_hw_exception_save("reserved", regs, 0U);
}
