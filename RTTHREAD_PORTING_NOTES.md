# STM32MP135 IAR 工程 RT-Thread 移植记录

记录时间：2026-05-14

## 移植目标

在当前 STM32MP135 IAR EWARM 工程中接入 `Middleware/RT-Thread` 下的 RT-Thread 最小内核，使工程能够启动 RT-Thread 调度器，并使用 Cortex-A7 PL1 Secure Physical Timer 作为 RT-Thread tick。

约束：

- FSBL/boot 阶段已经完成系统时钟初始化，不在应用工程里重复做整套时钟配置。
- STGEN/PL1 频率按 FSBL 配置适配，当前 FSBL 中 STGEN 选择 HSE，并执行了 `PL1_SetCounterFrequency(HSE_VALUE)`。
- 当前移植是最小内核版本，暂未启用 heap、console、finsh、device 组件。

## 新增文件

- `User/Core/Inc/rtconfig.h`
  - RT-Thread 最小配置。
  - `RT_TICK_PER_SECOND` 为 1000。
  - 单核配置：`RT_CPUS_NR 1`。
  - 静态 main 线程栈：`RT_MAIN_THREAD_STACK_SIZE 1024`。
  - 启用 `RT_USING_NANO`。

- `User/Core/Inc/board.h`
  - RT-Thread board 层声明。
  - tick 中断优先级定义为 `RT_TICK_IRQ_PRIORITY 0xA0U`，与已有 uCOS 工程保持一致。

- `User/Core/Src/rtthread_startup.c`
  - 自定义 RT-Thread 启动流程。
  - 初始化 board、timer、scheduler、main 线程、idle 线程、defunct 线程。
  - 使用静态 main 线程，入口通过弱符号 `rt_user_main_entry()` 对接用户应用。

- `User/Core/Src/rtthread_board.c`
  - 实现 `rt_hw_board_init()`。
  - 实现 PL1 tick 初始化和 `rt_hw_tick_handler()`。
  - 覆盖 `HAL_InitTick()`，避免 HAL 抢占 tick。
  - 实现 GIC IRQ 分发 `rt_hw_trap_irq()`。
  - 实现 RT-Thread 线程栈初始化 `rt_hw_stack_init()`。
  - 定义 RT-Thread 上下文切换所需的全局变量。

- `User/Core/Src/rtthread_port_iar.s`
  - IAR ASM Cortex-A7 上下文切换移植。
  - 实现：
    - `rt_hw_interrupt_disable`
    - `rt_hw_interrupt_enable`
    - `rt_hw_context_switch_to`
    - `rt_hw_context_switch`
    - `rt_hw_context_switch_interrupt`
    - `rt_hw_context_switch_exit`
    - `RTThread_IRQ_Handler`
    - `_thread_start`
  - 对接异常入口：
    - `FreeRTOS_SWI_Handler`
    - `Undef_Handler`
    - `PAbt_Handler`
    - `DAbt_Handler`
    - `FIQ_Handler`

- `User/Core/Src/rtthread_trap.c`
  - 实现 RT-Thread 风格异常处理函数：
    - `rt_hw_trap_swi`
    - `rt_hw_trap_undef`
    - `rt_hw_trap_pabt`
    - `rt_hw_trap_dabt`
    - `rt_hw_trap_resv`
  - 保存异常现场到全局变量，便于 IAR 调试器查看：
    - `rt_hw_exception_stack`
    - `rt_hw_exception_name`
    - `rt_hw_exception_fault_pc`
    - `rt_hw_exception_pc_adjust`

## 修改文件

- `User/Core/Src/main.c`
  - `main()` 改为调用 `rtthread_startup()`。
  - 新增 `rt_user_main_entry()`，当前用于周期翻转 LED：

```c
while (1)
{
    BSP_LED_Toggle();
    rt_thread_mdelay(300);
}
```

- `Drivers/CMSIS/Device/Source/IAR/startup_stm32mp135dxx_ca7_rtos.s`
  - IRQ 向量入口指向 `RTThread_IRQ_Handler`。
  - 异常向量改为引用当前 port 中实现的：
    - `Undef_Handler`
    - `PAbt_Handler`
    - `DAbt_Handler`

- `EWARM/MP135.ewp`
  - 增加 RT-Thread include path：
    - `Middleware/RT-Thread/include`
    - `Middleware/RT-Thread/libcpu/arm/cortex-a`
  - 增加宏：`__RT_KERNEL_SOURCE__`。
  - 加入用户 port 文件：
    - `rtthread_board.c`
    - `rtthread_startup.c`
    - `rtthread_trap.c`
    - `rtthread_port_iar.s`
  - 加入 RT-Thread kernel 源码：
    - `clock.c`
    - `cpu_up.c`
    - `defunct.c`
    - `idle.c`
    - `irq.c`
    - `ipc.c`
    - `kservice.c`
    - `klibc/kstdio.c`
    - `klibc/kstring.c`
    - `object.c`
    - `scheduler_up.c`
    - `scheduler_comm.c`
    - `thread.c`
    - `timer.c`
  - 将 CMSIS system 源文件切换为 `system_stm32mp13xx_A7.c`。
  - Release 配置补齐 Cortex-A7/FPU/NEON/linker/entry 设置，使 Debug 和 Release 都可构建。

## Tick 适配

RT-Thread tick 使用 `SecurePhyTimer_IRQn`。

初始化流程位于 `rt_hw_tick_init()`：

1. 关闭 PL1 timer。
2. 根据当前 STGEN 源选择 HSE/HSI 频率。
3. 设置 PL1 counter frequency。
4. 设置 reload 值：`timer_frequency / RT_TICK_PER_SECOND`。
5. 注册 `rt_hw_tick_handler()` 到 GIC。
6. 设置优先级 `0xA0`。
7. 设置 edge trigger。
8. 使能 GIC IRQ 和 PL1 timer。

tick handler 中会重新装载：

```c
PL1_SetLoadValue(reload + PL1_GetCurrentValue());
rt_tick_increase();
```

这样可以补偿一部分中断进入延迟。

## 上下文切换关键点

### 首次切线程必须进入 SVC mode

启动流程中 `rtthread_startup()` 会先关闭 IRQ，再启动调度器。第一次 `rt_hw_context_switch_to()` 不是从异常上下文进入，因此需要先切到 SVC mode，再通过 `SPSR` 恢复线程 CPSR。

当前处理：

```asm
rt_hw_context_switch_to
        CLREX
        MSR     CPSR_c, #0xD3
        LDR     SP, [R0]
        B       rt_hw_context_switch_exit
```

### 初始线程 CPSR 固定为 ARM SVC

IAR 下 C 函数通常是 Thumb 奇地址，但初始 PC 是 ARM 汇编函数 `_thread_start`，不是 C 入口。因此 `rt_hw_stack_init()` 中初始 CPSR 必须保持 ARM SVC：

```c
*(--stk) = 0x13U;
```

然后 `_thread_start` 使用 `BLX R1` 跳转到真正线程入口，由硬件根据入口地址 bit0 切换到 Thumb。

这个问题曾导致线程首次启动后上下文跑乱，表现为 data abort，fault PC 落在 `timer.o` 的 `rt_list_remove()`，本质是线程上下文恢复错误。

## 异常处理

异常入口在 `rtthread_port_iar.s` 中保存 `rt_hw_exp_stack` 格式现场，然后调用 `rtthread_trap.c` 中的 C handler。

异常现场结构保存字段：

- r0-r10
- fp
- ip
- sp
- lr
- pc
- cpsr

`rt_hw_exception_fault_pc` 会按异常类型修正：

- SWI/Undefined/Prefetch Abort：`pc - 4`
- Data Abort：`pc - 8`

当前未启用 `RT_USING_CONSOLE`，所以 `rt_kprintf()` 不会输出；调试时直接在 IAR Watch 中查看全局变量。

## 已定位并修复的问题

### 1. PL1 tick 不进 handler

排查方向：

- 对比 uCOS 工程 PL1 timer。
- 确认使用 `SecurePhyTimer_IRQn`。
- 确认 GIC IRQ 总入口进入 `RTThread_IRQ_Handler`。
- 检查 CPU CPSR I bit。

修复内容：

- 首次线程切换前切入 SVC mode。
- tick IRQ priority 改为 `0xA0`。
- IRQ 分发改为循环取完 pending IRQ。

### 2. 异常现场被覆盖

原因：

- SWI/异常入口在当前异常栈上构造现场后，过早恢复 SP，随后调用 C handler 时压栈覆盖了现场。

修复：

- 调 C handler 前保留异常现场所在栈空间，不再提前恢复 SP。

### 3. Data Abort 位于 `rt_list_remove()`

现场：

```text
exception: data abort
fault_pc: 0xc0000b26
lr:       0xc0001697
sp:       0xdf37b438
```

map 对应：

```text
0xc0000b23  rt_list_remove()   timer.o
0xc0001697  _thread_exit()     thread.o
0xdf37b438  idle_thread_stack
```

根因：

- 初始线程 CPSR 错误地根据 C 入口地址设置了 Thumb bit。
- 实际初始 PC 是 ARM 汇编 `_thread_start`，导致线程上下文启动异常。

修复：

- `rt_hw_stack_init()` 初始 CPSR 固定为 ARM SVC。

## 构建验证

使用绝对路径构建，避免 `$PROJ_DIR$` 解析到错误目录：

```powershell
iarbuild C:\Users\Kamisato\Desktop\folder\test\EWARM\MP135.ewp -build Debug
iarbuild C:\Users\Kamisato\Desktop\folder\test\EWARM\MP135.ewp -build Release
```

当前结果：

```text
Debug:   0 errors, 2 warnings, Build succeeded
Release: 0 errors, 2 warnings, Build succeeded
```

剩余 warning 来自 RT-Thread 源码：

```text
Middleware\RT-Thread\src\ipc.c(82): _ipc_object_init declared but never referenced
Middleware\RT-Thread\src\object.c(724): enumerated type mixed with another type
```

## 上板验证建议

1. 烧录 Debug。
2. 确认不再进入 `rt_hw_trap_dabt()`。
3. 确认 `RTThread_IRQ_Handler -> rt_hw_trap_irq -> rt_hw_tick_handler` 可周期进入。
4. 确认 `rt_user_main_entry()` 中 LED 以约 300 ms 周期翻转。
5. 若再次异常，优先查看：
   - `rt_hw_exception_name`
   - `rt_hw_exception_fault_pc`
   - `rt_hw_exception_stack`
   - `rt_thread_self()`
   - 当前线程 `sp`

## 当前限制和后续方向

- 未启用 `RT_USING_CONSOLE`，RT-Thread banner 和异常打印不会输出。
- 未启用 heap，当前线程为静态创建。
- 未接入 finsh/device/lwIP 等组件。
- 如需扩展组件，建议按以下顺序：
  1. 接 UART console，实现 `rt_hw_console_output()`。
  2. 打开 `RT_USING_CONSOLE`。
  3. 接 heap，启用动态线程/对象。
  4. 再逐步加入 device、finsh、网络组件。
