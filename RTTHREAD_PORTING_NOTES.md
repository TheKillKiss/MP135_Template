# STM32MP135 IAR 工程 RT-Thread 移植记录

记录日期：2026-05-15

## 移植目标

在当前 STM32MP135 IAR EWARM 工程中接入 `Middleware/RT-Thread` 下的 RT-Thread 内核、组件初始化系统、device 框架、UART4 shell，以及基于 eMMC/W25Q128 的 RTT DFS 文件系统。

约束：
- FSBL/boot 阶段已经完成系统时钟初始化，应用工程不重复做整套时钟树配置。
- RT-Thread tick 使用 Cortex-A7 PL1 Secure Physical Timer，并根据当前 STGEN 时钟源适配 tick 频率。
- shell 使用 UART4：PD8/UART4_RX、PD6/UART4_TX，RX 为中断模式。

## 当前核心配置

`User/Core/Inc/rtconfig.h` 当前启用：
- `RT_USING_USER_MAIN`
- `RT_USING_COMPONENTS_INIT`
- `RT_USING_HEAP`
- `RT_USING_SMALL_MEM_AS_HEAP`
- `RT_HEAP_SIZE (128 * 1024)`
- `RT_USING_DEVICE`
- `RT_USING_DEVICE_OPS`
- `RT_USING_SERIAL`
- `RT_USING_SERIAL_V1`
- `RT_USING_MTD_NOR`
- `RT_USING_DFS`
- `RT_USING_DFS_V1`
- `DFS_USING_POSIX`
- `DFS_USING_WORKDIR`
- `RT_USING_DFS_ELMFAT`
- `RT_DFS_ELM_DRIVES 2`
- `RT_CONSOLE_DEVICE_NAME "uart4"`
- `RT_USING_MSH`
- `RT_USING_FINSH`

## 启动链路

当前工程使用 RT-Thread 原生 `components.c` 启动链，`main()` 运行在 RT-Thread main 线程中：

```text
Reset_Handler
  -> __cmain
  -> components.c::__low_level_init()
  -> __iar_data_init3()
  -> SystemInit()
  -> components.c::rtthread_startup()
  -> rt_hw_board_init()
  -> rt_application_init()
  -> rt_system_scheduler_start()
  -> main thread
  -> components.c::main_thread_entry()
  -> rt_components_init()
  -> User/Core/Src/main.c::main()
```

`User/Core/Src/rtthread_startup.c` 已删除，启动统一由 `Middleware/RT-Thread/src/components.c` 接管。

## Tick 适配

tick 初始化位于 `User/Core/Src/rtthread_board.c`：
- 使用 `SecurePhyTimer_IRQn`。
- 根据 `RCC->STGENCKSELR` 判断 STGEN 当前使用 HSE/HSI。
- `PL1_SetCounterFrequency()` 使用当前 timer frequency。
- reload 为 `timer_frequency / RT_TICK_PER_SECOND`。
- IRQ priority 为 `RT_TICK_IRQ_PRIORITY 0xA0U`。
- `rt_hw_tick_handler()` 中清 pending、重装 PL1 compare/load，并调用 `rt_tick_increase()`。

`HAL_InitTick()` 被覆盖为空实现，避免 HAL 抢占 tick。

## Shell / UART4

`User/Core/Src/rtthread_shell_port.c` 将 UART4 注册为 RTT serial 设备：
- 设备名：`uart4`
- console：`RT_CONSOLE_DEVICE_NAME "uart4"`
- UART 参数：115200 8N1
- 引脚：PD8 RX、PD6 TX，复用 `GPIO_AF8_UART4`
- RX：`RT_DEVICE_FLAG_INT_RX`
- TX：polling 输出
- 初始化挂入：`INIT_BOARD_EXPORT(rt_hw_uart4_console_init)`

UART4 IRQ 中调用：

```c
rt_hw_serial_isr(&uart4_serial, RT_SERIAL_EVENT_RX_IND);
```

FinSH/MSH 由 `INIT_APP_EXPORT(finsh_system_init)` 自动创建 shell 线程。

## 异常与上下文

`Drivers/CMSIS/Device/Source/IAR/startup_stm32mp135dxx_ca7_rtos.s`：
- IRQ 向量入口指向 `RTThread_IRQ_Handler`
- SWI 向量指向 `SWI_Handler`
- Undefined/Prefetch Abort/Data Abort 向量接入 RTT port 中的异常入口

`User/Core/Src/rtthread_trap.c`：
- 保存异常现场到全局变量，便于 IAR Watch 查看
- 记录 `rt_hw_exception_name`
- 记录修正后的 `rt_hw_exception_fault_pc`
- 保留 `rt_hw_exception_stack`

首次线程切换修复点：
- `rt_hw_context_switch_to()` 先切到 SVC mode。
- `rt_hw_stack_init()` 初始 CPSR 固定为 ARM SVC：`0x13U`。
- `_thread_start` 使用 `BLX` 跳转到真实线程入口，由入口地址 bit0 决定 ARM/Thumb。

## Device 框架

已接入 RT-Thread device/device ops：
- `components/drivers/core/device.c`
- `components/drivers/ipc/completion_comm.c`
- `components/drivers/ipc/completion_up.c`
- `components/drivers/serial/dev_serial.c`
- `components/drivers/mtd/mtd_nor.c`

额外手写的 device MSH 命令已去掉，只保留 RT-Thread 原生：

```text
list device
```

## 文件系统移植

本次新增基于参考工程 `MP135_SYS` 中已验证 BSP 驱动的 RTT 文件系统适配。

新增 BSP 文件：
- `Drivers/BSP/Inc/bsp_spi.h`
- `Drivers/BSP/Src/bsp_spi.c`
- `Drivers/BSP/Inc/bsp_w25q128.h`
- `Drivers/BSP/Src/bsp_w25q128.c`
- `Drivers/BSP/Inc/bsp_emmc.h`
- `Drivers/BSP/Src/bsp_emmc.c`

新增 RTT 适配文件：
- `User/Core/Src/rtthread_filesystem_port.c`

### eMMC

eMMC 使用 SDMMC2：
- 8-bit bus
- `hmmc2`
- SDMMC2 IRQ：`User/Core/Src/stm32mp13xx_it.c::SDMMC2_IRQHandler()`
- IRQ handler 调用 `HAL_MMC_IRQHandler(&hmmc2)`
- GIC handler 在 `Drivers/BSP/Src/bsp_emmc.c` 中通过 `IRQ_SetHandler(SDMMC2_IRQn, bsp_emmc_irq_handler)` 注册，触发方式为 level。
- BSP 读写使用 `HAL_MMC_ReadBlocks_DMA()` / `HAL_MMC_WriteBlocks_DMA()`
- 通过 HAL callback 等待 DMA 完成

RTT 设备：
- block 设备名：`emmc0`
- device class：`RT_Device_Class_Block`
- 支持 `RT_DEVICE_CTRL_BLK_GETGEOME`
- 支持 `RT_DEVICE_CTRL_BLK_SYNC`
- block 层不开放整片擦除，避免误调用破坏已有数据

启动后会尝试：

```c
dfs_mount("emmc0", "/", "elm", 0, 0);
```

如果 eMMC 上还没有 FAT 文件系统，会打印提示，需要在 shell 中手动格式化：

```text
mkfs -t elm emmc0
```

随后复位或手动挂载：

```text
mount emmc0 / elm
```

### W25Q128

W25Q128 使用 SPI5：
- PH12：SPI5_MOSI
- PH7：SPI5_SCK
- PE4：SPI5_MISO
- PI1：软件片选 NSS

RTT 设备：
- MTD NOR 设备名：`w25q`
- 逻辑 block 设备名：`w25q0`
- W25Q block 设备使用 512B 逻辑扇区
- 写入时按 4 KiB erase block 做 read-modify-erase-write，避免 FAT 小块写破坏同一擦除块内其他数据

W25Q 默认只注册设备，不自动挂载。eMMC 根文件系统挂载成功后，可在 shell 中操作：

```text
mkdir /w25q
mkfs -t elm w25q0
mount w25q0 /w25q elm
```

### DFS / elmfat

IAR 工程已加入：
- `components/dfs/dfs_v1/src/dfs.c`
- `components/dfs/dfs_v1/src/dfs_file.c`
- `components/dfs/dfs_v1/src/dfs_fs.c`
- `components/dfs/dfs_v1/src/dfs_posix.c`
- `components/dfs/dfs_v1/filesystems/elmfat/dfs_elm.c`
- `components/dfs/dfs_v1/filesystems/elmfat/ff.c`
- `components/dfs/dfs_v1/filesystems/elmfat/ffunicode.c`
- `components/finsh/msh_file.c`
- `components/libc/compilers/common/ctime.c`

IAR include path 已补充：
- `Middleware/RT-Thread/components/dfs/dfs_v1/include`
- `Middleware/RT-Thread/components/dfs/dfs_v1/filesystems/elmfat`
- `Middleware/RT-Thread/components/libc/compilers/common/include`
- `Middleware/RT-Thread/components/libc/compilers/common/extension`
- `Middleware/RT-Thread/components/libc/compilers/common/extension/fcntl/octal`

可用 shell 命令包括：
- `ls`
- `cd`
- `pwd`
- `mkdir`
- `mkfs`
- `mount`
- `umount`
- `df`
- `cat`
- `cp`
- `mv`
- `rm`
- `echo`
- `tail`

## POSIX 兼容头

为适配 DFS/elmfat，当前工程补充了轻量 POSIX 兼容头：
- `User/Core/Inc/sys/types.h`
- `User/Core/Inc/sys/errno.h`

其中新增了 DFS 需要的类型和错误码，例如：
- `ssize_t`
- `off_t`
- `mode_t`
- `uid_t`
- `gid_t`
- `useconds_t`
- `suseconds_t`
- `ENOENT`
- `EINVAL`
- `ENOMEM`
- `ENOSYS`
- `EIO`
- `EFAULT`

## IAR 工程改动

`EWARM/MP135.ewp` 已加入：
- 新增 BSP 源文件
- `rtthread_filesystem_port.c`
- HAL SPI 源文件 `stm32mp13xx_hal_spi.c`
- RTT MTD NOR 源文件
- DFS v1 + elmfat 源文件
- FinSH 文件命令源文件
- RT-Thread common time 源文件
- DFS/elmfat/libc include path

## 构建验证

使用：

```powershell
iarbuild C:\Users\Kamisato\Desktop\folder\test\EWARM\MP135.ewp -build Debug
iarbuild C:\Users\Kamisato\Desktop\folder\test\EWARM\MP135.ewp -build Release
```

最近验证结果：

```text
Debug:  0 errors, 0 warnings, Build succeeded
Release: 0 errors, 0 warnings, Build succeeded
```

## 上板验证建议

1. 连接 UART4，115200 8N1。
2. 确认能看到 RT-Thread banner 和 `msh >`。
3. 输入 `list device`，应能看到 `uart4`、`emmc0`，W25Q 存在时应看到 `w25q`、`w25q0`。
4. 如果 eMMC 未格式化，执行 `mkfs -t elm emmc0`，然后复位或执行 `mount emmc0 / elm`。
5. 验证 eMMC：
```text
echo hello > /hello.txt
ls /
cat /hello.txt
df /
```
6. 验证 W25Q：
```text
mkdir /w25q
mkfs -t elm w25q0
mount w25q0 /w25q elm
echo flash > /w25q/test.txt
ls /w25q
cat /w25q/test.txt
df /w25q
```

## 当前限制

- W25Q 上的 FAT 通过 4 KiB read-modify-erase-write 适配，能用但速度不适合高频日志写入；后续如果要长期写日志，更建议接入 FAL + littlefs。
- eMMC 目前注册为整盘 `emmc0`，未做 GPT/MBR 分区扫描；当前挂载整盘 FAT。
- 自动挂载不会自动格式化，避免误擦已有数据。

## 存储设备初始化开关

存储设备是否参与启动初始化由 `User/Core/Inc/rtconfig.h` 中的 BSP 宏控制：

```c
#define BSP_USING_EMMC
#define BSP_USING_W25Q128
#define BSP_USING_W25Q128_BLOCK
#define BSP_USING_FS_AUTO_MOUNT_EMMC
/* #define BSP_USING_FS_AUTO_MOUNT_W25Q */
```

- `BSP_USING_EMMC`：控制 `BSP_EMMC_Init()`、`emmc0` block 设备注册。注释后启动时不会初始化 eMMC，也不会注册 `emmc0`。
- `BSP_USING_W25Q128`：控制 `BSP_SPI5_Init()`、`BSP_W25Q128_Init()`、`w25q` MTD NOR 设备注册。注释后启动时不会初始化 W25Q128。
- `BSP_USING_W25Q128_BLOCK`：控制 `w25q0` 逻辑 block 设备注册。该宏依赖 `BSP_USING_W25Q128`，只关闭它时仍会保留 `w25q` MTD 设备。
- `BSP_USING_FS_AUTO_MOUNT_EMMC`：控制启动后是否自动执行 `dfs_mount("emmc0", "/", "elm", 0, 0)`。只关闭它时，eMMC 仍会初始化并注册为 `emmc0`，但根文件系统需要在 shell 中手动挂载。
- `BSP_USING_FS_AUTO_MOUNT_W25Q`：控制启动后是否自动执行 `dfs_mount("w25q0", "/", "elm", 0, 0)`。该宏依赖 `BSP_USING_W25Q128_BLOCK`。

`BSP_USING_FS_AUTO_MOUNT_EMMC` 和 `BSP_USING_FS_AUTO_MOUNT_W25Q` 不能同时开启，因为两者都会尝试挂载到根目录 `/`。

常用组合：

```c
/* 只禁用 eMMC，保留 W25Q */
/* #define BSP_USING_EMMC */
#define BSP_USING_W25Q128
#define BSP_USING_W25Q128_BLOCK
#define BSP_USING_FS_AUTO_MOUNT_W25Q

/* 只注册 eMMC，不自动挂载根文件系统 */
#define BSP_USING_EMMC
/* #define BSP_USING_FS_AUTO_MOUNT_EMMC */

/* eMMC 作为根文件系统 */
#define BSP_USING_EMMC
#define BSP_USING_FS_AUTO_MOUNT_EMMC
/* #define BSP_USING_FS_AUTO_MOUNT_W25Q */
```
