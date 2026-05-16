# STM32MP135 IAR 工程 RT-Thread 移植教程

本文记录当前 `RTT` 分支从基础工程移植到当前状态的完整代码级步骤。目标读者可以不了解 RT-Thread，但需要具备 STM32MP135、IAR EWARM、HAL、启动文件和中断向量的基础概念。

当前最终状态：

- RT-Thread 版本：`5.2.2`
- 工程：STM32MP135 Cortex-A7，IAR EWARM
- 启动方式：IAR `__low_level_init()` 进入 RT-Thread 原生 `rtthread_startup()`
- 系统 tick：A7 PL1 Secure Physical Timer，适配 FSBL/boot 已初始化好的时钟
- 组件系统：使用 RT-Thread 原生 `components.c`
- device 框架：开启 `RT_USING_DEVICE`、`RT_USING_DEVICE_OPS`
- shell：FinSH/MSH，UART4，PD8 RX，PD6 TX，中断接收
- 文件系统：DFS v1 + elmfat
- 存储设备：
  - `emmc0`：eMMC user 区 block device
  - `w25q`：W25Q128 MTD NOR device
  - `w25q0`：W25Q128 512 字节逻辑 block device，用于 FAT/DFS
- 网络：RT-Thread 自带 lwIP 2.1.2，ETH1 + YT8531C，网卡设备 `e0`
- 网络测试：lwIP 自带 `lwiperf` 封装为 MSH 命令 `iperf`，兼容 iperf2 TCP 测试

## 1. 分支修改记录

当前分支从 `4923f37` 之后形成 RTT 移植线：

| 提交 | 主题 | 主要内容 |
| --- | --- | --- |
| `322dfc1` | `base` | 引入 RT-Thread 基础内核、A7 IAR 汇编上下文切换、PL1 tick、异常现场保存、启动向量接入 RTT IRQ。 |
| `f10d351` | `deivce_uart4` | 引入 device/serial/FinSH，UART4 注册为 RTT serial console，shell 通过 UART4 中断接收。 |
| `e5931dc` | `comp+device 修复警告 开启cache` | 切到 RTT 原生 `components.c` 启动链；删除自定义 startup C 文件；补 POSIX 兼容头；修复 IAR `Pe188` 枚举混用警告；更新 IAR 工程配置。 |
| `3519a41` | `DFSv1` | 引入 DFS v1、elmfat、W25Q128/eMMC BSP，注册 RTT block/MTD 设备并支持自动挂载。 |
| 当前工作区 | `lwIP/ETH1` | 引入 RTT lwIP 2.1.2、ETH1 BSP、YT8531C PHY，支持静态 IP/DHCP、ping、RX custom pbuf、cache 维护和 `ETH_PAD_SIZE=2` 对齐。 |

涉及文件：

```text
User/Core/Inc/rtconfig.h
User/Core/Inc/board.h
User/Core/Inc/sys/errno.h
User/Core/Inc/sys/types.h
User/Core/Src/main.c
User/Core/Src/rtthread_board.c
User/Core/Src/rtthread_port_iar.s
User/Core/Src/rtthread_shell_port.c
User/Core/Src/rtthread_filesystem_port.c
User/Core/Src/rtthread_iperf_port.c
User/Core/Src/rtthread_trap.c
User/Core/Src/stm32mp13xx_it.c
Drivers/BSP/Inc/bsp_spi.h
Drivers/BSP/Inc/bsp_w25q128.h
Drivers/BSP/Inc/bsp_emmc.h
Drivers/BSP/Inc/bsp_eth.h
Drivers/BSP/Inc/bsp_yt8531c.h
Drivers/BSP/Src/bsp_spi.c
Drivers/BSP/Src/bsp_w25q128.c
Drivers/BSP/Src/bsp_emmc.c
Drivers/BSP/Src/bsp_eth.c
Drivers/BSP/Src/bsp_yt8531c.c
Drivers/CMSIS/Device/Source/IAR/startup_stm32mp135dxx_ca7_rtos.s
Drivers/CMSIS/Device/Source/IAR/linker/stm32mp13xx_a7_ddr.icf
Middleware/RT-Thread/src/object.c
EWARM/MP135.ewp
```

## 2. 准备源码目录

在基础工程根目录放入 RT-Thread 源码：

```text
Middleware/RT-Thread
```

当前工程使用到的 RTT 子目录：

```text
Middleware/RT-Thread/include
Middleware/RT-Thread/libcpu/arm/cortex-a
Middleware/RT-Thread/src
Middleware/RT-Thread/components/finsh
Middleware/RT-Thread/components/drivers
Middleware/RT-Thread/components/dfs/dfs_v1
Middleware/RT-Thread/components/libc/compilers/common
Middleware/RT-Thread/components/net/lwip
```

额外 BSP 驱动来自旧工程或外部驱动目录，最终放入当前工程：

```text
Drivers/BSP/Inc/bsp_spi.h
Drivers/BSP/Inc/bsp_w25q128.h
Drivers/BSP/Inc/bsp_emmc.h
Drivers/BSP/Inc/bsp_eth.h
Drivers/BSP/Inc/bsp_yt8531c.h
Drivers/BSP/Src/bsp_spi.c
Drivers/BSP/Src/bsp_w25q128.c
Drivers/BSP/Src/bsp_emmc.c
Drivers/BSP/Src/bsp_eth.c
Drivers/BSP/Src/bsp_yt8531c.c
```

## 3. IAR 工程配置

### 3.1 预编译宏

Debug 配置加入：

```text
USE_HAL_DRIVER
STM32MP135Dxx
CORE_CA7
NO_MMU_USE
DDR_TYPE_DDR3_4Gb
__RT_KERNEL_SOURCE__
```

当前 Debug 已移除 `NO_CACHE_USE`，让 `system_stm32mp13xx_A7.c` 可以按 `CACHE_USE` 相关逻辑开启 cache。Release 配置当前仍保留 `NO_CACHE_USE`，如果要让 Release 也开启 cache，需要同步移除。

`__RT_KERNEL_SOURCE__` 很关键。它会让 RT-Thread 头文件暴露内核编译需要的声明，缺少时容易出现类型或导出宏问题。

### 3.2 Include path

Debug 和 Release 都加入：

```text
$PROJ_DIR$\..\Middleware\RT-Thread\include
$PROJ_DIR$\..\Middleware\RT-Thread\libcpu\arm\cortex-a
$PROJ_DIR$\..\Middleware\RT-Thread\components\finsh
$PROJ_DIR$\..\Middleware\RT-Thread\components\drivers\include
$PROJ_DIR$\..\Middleware\RT-Thread\components\dfs\dfs_v1\include
$PROJ_DIR$\..\Middleware\RT-Thread\components\dfs\dfs_v1\filesystems\elmfat
$PROJ_DIR$\..\Middleware\RT-Thread\components\libc\compilers\common\include
$PROJ_DIR$\..\Middleware\RT-Thread\components\libc\compilers\common\extension
$PROJ_DIR$\..\Middleware\RT-Thread\components\libc\compilers\common\extension\fcntl\octal
$PROJ_DIR$\..\Middleware\RT-Thread\components\net\lwip\port
$PROJ_DIR$\..\Middleware\RT-Thread\components\net\lwip\port\arch
$PROJ_DIR$\..\Middleware\RT-Thread\components\net\lwip\port\netif
$PROJ_DIR$\..\Middleware\RT-Thread\components\net\lwip\lwip-2.1.2\src\include
$PROJ_DIR$\..\Middleware\RT-Thread\components\net\lwip\lwip-2.1.2\src\include\netif
$PROJ_DIR$\..\Middleware\RT-Thread\components\net\lwip\lwip-2.1.2\src\include\compat\posix
```

### 3.3 IAR 入口和链接脚本

工程使用：

```text
Ilink program entry: Reset_Handler
Ilink ICF file: Drivers\CMSIS\Device\Source\IAR\linker\stm32mp13xx_a7_ddr.icf
```

链接脚本中关键点：

```c
define symbol __ICFEDIT_intvec_start__ = 0xC0000000;
place at address mem:__ICFEDIT_intvec_start__ { readonly section .intvec };

define symbol __ICFEDIT_size_svcstack__ = 0x400;
define symbol __ICFEDIT_size_irqstack__ = 0x400;
define symbol __ICFEDIT_size_fiqstack__ = 0x400;
define symbol __ICFEDIT_size_undstack__ = 0x400;
define symbol __ICFEDIT_size_abtstack__ = 0x400;
```

RT-Thread 的自动初始化表是 `.rti_fn.*` 只读段，当前 ICF 把只读数据放入 DDR1：

```c
define block RO_CODE { ro code };
define block RO_DATA with alignment = 4K { ro data };
place in DDR1_region  { first block RO_CODE, block RO_DATA };
```

因此不需要额外手工放置 `.rti_fn.*`。

### 3.4 加入工程源文件

应用层新增：

```text
User/Core/Src/rtthread_board.c
User/Core/Src/rtthread_port_iar.s
User/Core/Src/rtthread_shell_port.c
User/Core/Src/rtthread_filesystem_port.c
User/Core/Src/rtthread_iperf_port.c
User/Core/Src/rtthread_trap.c
```

BSP 新增：

```text
Drivers/BSP/Src/bsp_spi.c
Drivers/BSP/Src/bsp_w25q128.c
Drivers/BSP/Src/bsp_emmc.c
Drivers/BSP/Src/bsp_eth.c
Drivers/BSP/Src/bsp_yt8531c.c
```

RT-Thread 内核源文件加入：

```text
Middleware/RT-Thread/src/clock.c
Middleware/RT-Thread/src/components.c
Middleware/RT-Thread/src/cpu_up.c
Middleware/RT-Thread/src/defunct.c
Middleware/RT-Thread/src/idle.c
Middleware/RT-Thread/src/ipc.c
Middleware/RT-Thread/src/irq.c
Middleware/RT-Thread/src/klibc/kerrno.c
Middleware/RT-Thread/src/kservice.c
Middleware/RT-Thread/src/klibc/kstdio.c
Middleware/RT-Thread/src/klibc/kstring.c
Middleware/RT-Thread/src/mem.c
Middleware/RT-Thread/src/object.c
Middleware/RT-Thread/src/scheduler_comm.c
Middleware/RT-Thread/src/scheduler_up.c
Middleware/RT-Thread/src/thread.c
Middleware/RT-Thread/src/timer.c
```

RT-Thread device/serial/MTD：

```text
Middleware/RT-Thread/components/drivers/ipc/completion_comm.c
Middleware/RT-Thread/components/drivers/ipc/completion_up.c
Middleware/RT-Thread/components/drivers/serial/dev_serial.c
Middleware/RT-Thread/components/drivers/core/device.c
Middleware/RT-Thread/components/drivers/mtd/mtd_nor.c
```

FinSH/MSH：

```text
Middleware/RT-Thread/components/finsh/cmd.c
Middleware/RT-Thread/components/finsh/msh.c
Middleware/RT-Thread/components/finsh/msh_parse.c
Middleware/RT-Thread/components/finsh/shell.c
Middleware/RT-Thread/components/finsh/msh_file.c
```

DFS v1 + elmfat：

```text
Middleware/RT-Thread/components/dfs/dfs_v1/src/dfs.c
Middleware/RT-Thread/components/dfs/dfs_v1/src/dfs_file.c
Middleware/RT-Thread/components/dfs/dfs_v1/src/dfs_fs.c
Middleware/RT-Thread/components/dfs/dfs_v1/src/dfs_posix.c
Middleware/RT-Thread/components/dfs/dfs_v1/filesystems/elmfat/dfs_elm.c
Middleware/RT-Thread/components/dfs/dfs_v1/filesystems/elmfat/ff.c
Middleware/RT-Thread/components/dfs/dfs_v1/filesystems/elmfat/ffunicode.c
```

LibC 辅助：

```text
Middleware/RT-Thread/components/libc/compilers/common/ctime.c
```

lwIP 2.1.2：

```text
Middleware/RT-Thread/components/net/lwip/port/ethernetif.c
Middleware/RT-Thread/components/net/lwip/port/sys_arch.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/altcp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/altcp_alloc.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/altcp_tcp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/def.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/dns.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/inet_chksum.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/init.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ip.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/memp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/netif.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/pbuf.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/raw.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/stats.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/sys.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/tcp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/tcp_in.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/tcp_out.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/timeouts.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/udp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/autoip.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/dhcp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/etharp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/icmp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/igmp.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/ip4.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/ip4_addr.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/core/ipv4/ip4_frag.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/api_lib.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/api_msg.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/err.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/if_api.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/netbuf.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/netdb.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/netifapi.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/sockets.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/api/tcpip.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/netif/ethernet.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/netif/lowpan6.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/apps/ping/ping.c
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/apps/lwiperf/lwiperf.c
```

## 4. 创建 rtconfig.h

新增 `User/Core/Inc/rtconfig.h`：

```c
#ifndef RTCONFIG_H__
#define RTCONFIG_H__

/* RT-Thread 对象名称最大长度，包含字符串结束符前的有效名称长度限制。 */
#define RT_NAME_MAX                    8
/* 内核对象、栈、堆等内存对齐粒度。 */
#define RT_ALIGN_SIZE                  8
/* 线程优先级总数，数值越小优先级越高。 */
#define RT_THREAD_PRIORITY_MAX         32
/* 系统 tick 频率，当前为 1000Hz，即 1ms 一个 tick。 */
#define RT_TICK_PER_SECOND             1000
/* CPU 核心数量，当前只启用 Cortex-A7 单核。 */
#define RT_CPUS_NR                     1

/* idle 空闲线程栈大小。 */
#define IDLE_THREAD_STACK_SIZE         512
/* RT-Thread 创建的 main 线程栈大小，用户 main() 在线程上下文中运行。 */
#define RT_MAIN_THREAD_STACK_SIZE      1024
/* main 线程优先级。 */
#define RT_MAIN_THREAD_PRIORITY        10
/* backtrace 最大回溯层数。 */
#define RT_BACKTRACE_LEVEL_MAX_NR      16
/* 使用标准 C 库的 vsnprintf 作为 RT-Thread kprintf 格式化后端。 */
#define RT_KLIBC_USING_LIBC_VSNPRINTF
/* 启用用户 main() 入口，IAR __low_level_init() 会进入 rtthread_startup() 再创建 main 线程。 */
#define RT_USING_USER_MAIN
/* 启用 RT-Thread 组件自动初始化机制，支持 INIT_xxx_EXPORT()。 */
#define RT_USING_COMPONENTS_INIT
/* 启用信号量 IPC 对象。 */
#define RT_USING_SEMAPHORE
/* 启用互斥量 IPC 对象。 */
#define RT_USING_MUTEX
/* 启用邮箱 IPC 对象，lwIP 的 tcpip/ethernetif 线程间投递消息会使用。 */
#define RT_USING_MAILBOX
/* 启用动态堆内存管理。 */
#define RT_USING_HEAP
/* 启用 small memory 小内存堆算法。 */
#define RT_USING_SMALL_MEM
/* 将 small memory 作为系统 heap 实现。 */
#define RT_USING_SMALL_MEM_AS_HEAP
/* 静态分配给 RT-Thread 的系统堆大小；当前链接脚本将 RW 放在 DDR2，先保留 256KB 给 lwIP/socket/DFS 使用。 */
#define RT_HEAP_SIZE                   (256 * 1024)

/* 启用 RT-Thread device 设备框架。 */
#define RT_USING_DEVICE
/* 使用 struct rt_device_ops 形式保存设备操作函数表。 */
#define RT_USING_DEVICE_OPS
/* 启用 MTD NOR 设备类型，用于注册 W25Q128 原始 NOR flash。 */
#define RT_USING_MTD_NOR
/* 启用串口设备框架。 */
#define RT_USING_SERIAL
/* 使用 serial v1 驱动框架。 */
#define RT_USING_SERIAL_V1
/* 串口中断接收 ringbuffer 大小。 */
#define RT_SERIAL_RB_BUFSZ             256

/* 启用 DFS 虚拟文件系统框架。 */
#define RT_USING_DFS
/* 使用 DFS v1，适合当前非 Smart 标准 RT-Thread 工程。 */
#define RT_USING_DFS_V1
/* 启用 open/read/write/close 等 POSIX 风格文件接口。 */
#define DFS_USING_POSIX
/* 启用当前工作目录支持。 */
#define DFS_USING_WORKDIR
/* 最大同时挂载的文件系统实例数量。 */
#define DFS_FILESYSTEMS_MAX            4
/* 最大可注册的文件系统类型数量，例如 elmfat、romfs 等。 */
#define DFS_FILESYSTEM_TYPES_MAX       4
/* 最大文件描述符数量。 */
#define DFS_FD_MAX                     16
/* DFS 路径最大长度。 */
#define DFS_PATH_MAX                   256
/* 启用 elmfat，即 FatFs 文件系统适配层。 */
#define RT_USING_DFS_ELMFAT
/* elmfat 最大逻辑驱动器数量，当前预留 eMMC 和 W25Q 两个盘。 */
#define RT_DFS_ELM_DRIVES              2
/* elmfat 支持的最大扇区大小，当前 eMMC/W25Q block 均按 512 字节扇区暴露。 */
#define RT_DFS_ELM_MAX_SECTOR_SIZE     512
/* 是否启用 FAT 长文件名，0 表示关闭以节省资源。 */
#define RT_DFS_ELM_USE_LFN             0
/* FatFs 代码页，936 表示简体中文 GBK 代码页。 */
#define RT_DFS_ELM_CODE_PAGE           936

/* Board storage device switches */
/* 启用 eMMC BSP 初始化，并注册 emmc0 block device。 */
#define BSP_USING_EMMC
/* 启用 W25Q128 BSP 初始化，并注册 w25q MTD NOR device。 */
#define BSP_USING_W25Q128
/* 将 W25Q128 额外包装成 512 字节扇区的 w25q0 block device，供 FAT/DFS 使用。 */
#define BSP_USING_W25Q128_BLOCK
/* 启动时自动将 emmc0 以 elmfat 挂载到根目录 /。 */
#define BSP_USING_FS_AUTO_MOUNT_EMMC
/* 启动时自动将 w25q0 以 elmfat 挂载到根目录 /；不能和 BSP_USING_FS_AUTO_MOUNT_EMMC 同时开启。 */
// #define BSP_USING_FS_AUTO_MOUNT_W25Q
/* 启用 ETH1 + YT8531C PHY BSP 初始化，并注册 RTT lwIP 网卡设备 e0。 */
#define BSP_USING_ETH
/* ETH 使用静态 IP 初始化。 */
#define BSP_ETH_IP_MODE_STATIC          0
/* ETH 使用 DHCP 初始化。 */
#define BSP_ETH_IP_MODE_DHCP            1
/* ETH IP 初始化模式：改成 BSP_ETH_IP_MODE_DHCP 即启用 DHCP，默认静态 IP。 */
#define BSP_ETH_IP_MODE                 BSP_ETH_IP_MODE_STATIC

/* 启用 RT-Thread 自带 lwIP 网络协议栈组件。 */
#define RT_USING_LWIP
/* 选择 lwIP 2.1.2 版本。 */
#define RT_USING_LWIP212
/* lwIP 版本号，RT-Thread port 层用于条件编译。 */
#define RT_USING_LWIP_VER_NUM          0x20102
/* lwIP 内存对齐粒度，ETH DMA buffer/cache 以 32 字节 cache line 对齐更稳妥。 */
#define RT_LWIP_MEM_ALIGNMENT          32
/* 以太网帧前预留 2 字节，使 14 字节以太网头后的 IP 头按 4 字节对齐，避免 Cortex-A7 unaligned data abort。 */
#define RT_LWIP_ETH_PAD_SIZE           2
/* ETH RX DMA 使用 custom pbuf 将 DMA buffer 直接交给 lwIP。 */
#define LWIP_SUPPORT_CUSTOM_PBUF       1
/* 启用 ICMP，支持 ping。 */
#define RT_LWIP_ICMP
/* 启用 DNS 客户端。 */
#define RT_LWIP_DNS
/* 启用 UDP 协议。 */
#define RT_LWIP_UDP
/* 启用 TCP 协议。 */
#define RT_LWIP_TCP
/* 启用 RAW socket，ping 命令需要。 */
#define RT_LWIP_RAW
/* 启用 RT-Thread lwIP ping 命令。 */
#define RT_LWIP_USING_PING
/* 启用 lwIP 自带 lwiperf，并导出 iperf MSH 测速命令。 */
#define RT_LWIP_USING_IPERF

#if (BSP_ETH_IP_MODE == BSP_ETH_IP_MODE_DHCP)
/* DHCP 模式：网卡初始地址为 0.0.0.0，由 DHCP 客户端获取 IP/网关/掩码。 */
#define RT_LWIP_DHCP
#elif (BSP_ETH_IP_MODE == BSP_ETH_IP_MODE_STATIC)
/* 静态 IP 模式：不启用 DHCP，使用下面固定地址。 */
#undef RT_LWIP_DHCP
#else
#error "Invalid BSP_ETH_IP_MODE"
#endif

/* 静态 IP 地址，固定为 192.168.6.6。 */
#define RT_LWIP_IPADDR                 "192.168.6.6"
/* 静态网关地址。 */
#define RT_LWIP_GWADDR                 "192.168.6.1"
/* 静态子网掩码。 */
#define RT_LWIP_MSKADDR                "255.255.255.0"
/* lwIP pbuf pool 数量。 */
#define RT_LWIP_PBUF_NUM               16
/* RAW PCB 数量。 */
#define RT_LWIP_RAW_PCB_NUM            4
/* UDP PCB 数量。 */
#define RT_LWIP_UDP_PCB_NUM            4
/* TCP PCB 数量。 */
#define RT_LWIP_TCP_PCB_NUM            4
/* TCP segment 数量。 */
#define RT_LWIP_TCP_SEG_NUM            16
/* TCP 发送缓冲区大小。 */
#define RT_LWIP_TCP_SND_BUF            4096
/* TCP 接收窗口大小。 */
#define RT_LWIP_TCP_WND                4096
/* lwIP tcpip 线程优先级。 */
#define RT_LWIP_TCPTHREAD_PRIORITY     12
/* lwIP tcpip 线程栈大小。 */
#define RT_LWIP_TCPTHREAD_STACKSIZE    2048
/* lwIP tcpip 线程邮箱大小。 */
#define RT_LWIP_TCPTHREAD_MBOX_SIZE    8
/* RT-Thread ethernetif 收发线程优先级。 */
#define RT_LWIP_ETHTHREAD_PRIORITY     13
/* RT-Thread ethernetif 收发线程栈大小。 */
#define RT_LWIP_ETHTHREAD_STACKSIZE    2048
/* RT-Thread ethernetif 收发线程邮箱大小。 */
#define RT_LWIP_ETHTHREAD_MBOX_SIZE    8

/* 启用 RT-Thread 控制台输出。 */
#define RT_USING_CONSOLE
/* 控制台输出缓冲区大小。 */
#define RT_CONSOLEBUF_SIZE             256
/* 控制台绑定的 device 名称，当前使用 UART4。 */
#define RT_CONSOLE_DEVICE_NAME         "uart4"

/* 启用 MSH 命令行 shell。 */
#define RT_USING_MSH
/* 启用 FinSH shell 组件。 */
#define RT_USING_FINSH
/* 让 FinSH 使用 MSH 风格命令行。 */
#define FINSH_USING_MSH
/* 启用符号表，支持 MSH_CMD_EXPORT/FINSH_FUNCTION_EXPORT 导出命令。 */
#define FINSH_USING_SYMTAB
/* 启用命令描述字符串。 */
#define FINSH_USING_DESCRIPTION
/* 启用 shell 历史命令，上下方向键可切换最近输入。 */
#define FINSH_USING_HISTORY
/* 保存的历史命令条数。 */
#define FINSH_HISTORY_LINES            5
/* shell 线程名称。 */
#define FINSH_THREAD_NAME              "tshell"
/* shell 线程优先级。 */
#define FINSH_THREAD_PRIORITY          20
/* shell 线程栈大小。 */
#define FINSH_THREAD_STACK_SIZE        2048
/* shell 单条命令最大长度。 */
#define FINSH_CMD_SIZE                 128
/* shell 命令最大参数个数。 */
#define FINSH_ARG_MAX                  10
/* 启用 MSH 内置命令，例如 help、list、cd、pwd 等。 */
#define MSH_USING_BUILT_IN_COMMANDS

#endif
```

几个关键开关的意义：

- `RT_USING_USER_MAIN`：IAR 的 `__low_level_init()` 会调用 `rtthread_startup()`，再创建 `main` 线程去执行用户 `main()`。
- `RT_USING_COMPONENTS_INIT`：启用 `INIT_BOARD_EXPORT`、`INIT_APP_EXPORT` 这套自动初始化机制。
- `RT_USING_DEVICE`：打开 RTT device 框架，否则 `list device` 和 `rt_device_register()` 都不可用。
- `RT_USING_SERIAL`：打开 RTT serial 框架，用于 UART4 shell。
- `RT_USING_DFS`、`RT_USING_DFS_V1`、`RT_USING_DFS_ELMFAT`：打开 DFS v1 和 FAT 文件系统。
- `RT_USING_LWIP`、`RT_USING_LWIP212`：打开 RTT 自带 lwIP 2.1.2 组件。
- `RT_USING_MAILBOX`：lwIP 的 `tcpip_thread`、RTT `ethernetif` 线程会使用邮箱收发消息。
- `RT_LWIP_ETH_PAD_SIZE 2`：以太网帧前预留 2 字节，让 IP 头 4 字节对齐，避免 A7 data abort。
- `RT_LWIP_USING_IPERF`：打开用户侧 `iperf` MSH 命令封装，底层使用 lwIP 自带 `lwiperf`。
- `BSP_USING_FS_AUTO_MOUNT_EMMC` 和 `BSP_USING_FS_AUTO_MOUNT_W25Q` 不能同时打开，因为两者都挂载到根目录 `/`。
- `BSP_ETH_IP_MODE`：选择 `BSP_ETH_IP_MODE_STATIC` 时使用 `192.168.6.6`，选择 `BSP_ETH_IP_MODE_DHCP` 时启用 DHCP。

## 5. 创建 board.h

新增 `User/Core/Inc/board.h`：

```c
#ifndef BOARD_H__
#define BOARD_H__

#include "stm32mp13xx_hal.h"

#define RT_TICK_IRQ_PRIORITY           (0xA0U)
#define RT_UART4_IRQ_PRIORITY          (0xA0U)

void rt_hw_board_init(void);
void rt_hw_tick_init(void);

#endif
```

## 6. 修改 main.c

基础工程原来通常在 `main()` 中 `HAL_Init()`、`BSP_LED_Init()`、`HAL_Delay()` 裸跑。移植后这些初始化交给 `rt_hw_board_init()`，`main()` 将在线程环境中执行。

`User/Core/Src/main.c` 改为：

```c
#include "main.h"
#include "bsp_led.h"
#include <rtthread.h>

void rt_user_main_entry(void)
{
    while (1)
    {
        BSP_LED_Toggle();
        rt_thread_mdelay(500);
    }
}

int main(void)
{
    rt_user_main_entry();
    return 0;
}

void Error_Handler(void)
{

    __disable_irq();
    while (1)
    {
        BSP_LED_Toggle();
        HAL_Delay(100);
    }

}
```

注意：此处 `main()` 不再主动调用 `HAL_Init()`，否则会和 RTT board 初始化重复。

## 7. 接管启动向量

修改 `Drivers/CMSIS/Device/Source/IAR/startup_stm32mp135dxx_ca7_rtos.s`。

把非 `AZURE_RTOS` 分支里的 FreeRTOS 异常入口替换成 RT-Thread 入口：

```asm
#else
  EXTERN Undef_Handler
  EXTERN SWI_Handler
  EXTERN PAbt_Handler
  EXTERN DAbt_Handler
  EXTERN RTThread_IRQ_Handler
  EXTERN FIQ_Handler
#endif
```

向量表也对应替换：

```asm
__vector:
  ARM

  LDR    PC, =Reset_Handler
#ifdef AZURE_RTOS
  LDR    PC, =__tx_undefined
  LDR    PC, =__tx_swi_interrupt
  LDR    PC, =__tx_prefetch_handler
  LDR    PC, =__tx_abort_handler
  NOP
  LDR    PC, =__tx_irq_handler
  LDR    PC, =__tx_fiq_handler
#else
  LDR    PC, =Undef_Handler
  LDR    PC, =SWI_Handler
  LDR    PC, =PAbt_Handler
  LDR    PC, =DAbt_Handler
  NOP
  LDR    PC, =RTThread_IRQ_Handler
  LDR    PC, =FIQ_Handler
#endif
```

保留 `Reset_Handler` 中的：

```asm
  LDR    R0, =__vector
  MCR    p15, 0, R0, c12, c0, 0
  ISB
```

这样异常向量基址 `VBAR` 会指向当前工程 `.intvec` 的 `__vector`。

## 8. 引入 RT-Thread 原生启动链

当前工程使用 `Middleware/RT-Thread/src/components.c` 的 IAR 启动入口。

关键代码：

```c
#elif defined(__ICCARM__)
extern void __iar_data_init3(void);
extern void SystemInit(void);
int __low_level_init(void)
{
    __iar_data_init3();
    SystemInit();
    rtthread_startup();
    return 0;
}
```

当前不再依赖 `startup_stm32mp135dxx_ca7_rtos.s::__iar_data_init_done` 调用 `SystemInit()`。该 hook 在当前工程里保留为空返回，避免 IAR runtime 路径变化时造成 `SystemInit()` 重复执行或提前打开中断。

`rtthread_startup()` 顺序：

```c
int rtthread_startup(void)
{
    rt_hw_local_irq_disable();

    rt_hw_board_init();
    rt_show_version();
    rt_system_timer_init();
    rt_system_scheduler_init();
    rt_application_init();
    rt_system_timer_thread_init();
    rt_thread_idle_init();
    rt_thread_defunct_init();
    rt_system_scheduler_start();

    return 0;
}
```

`rt_application_init()` 创建 `main` 线程，`main_thread_entry()` 先做组件初始化，再调用用户 `main()`：

```c
static void main_thread_entry(void *parameter)
{
    extern int main(void);
    RT_UNUSED(parameter);

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_init();
#endif

    main();
}
```

## 9. 板级初始化和 tick

新增 `User/Core/Src/rtthread_board.c`。

核心全局变量必须存在，汇编上下文切换会使用：

```c
rt_uint32_t rt_interrupt_from_thread;
rt_uint32_t rt_interrupt_to_thread;
rt_uint32_t rt_thread_switch_interrupt_flag;
```

使用静态堆：

```c
#ifdef RT_USING_HEAP
rt_align(RT_ALIGN_SIZE)
static rt_uint8_t rt_heap[RT_HEAP_SIZE];
#endif
```

tick 频率从当前 STGEN 时钟源推导。因为 FSBL/boot 已完成时钟配置，这里不重新配置系统时钟，只读取当前选择：

```c
static rt_uint32_t rt_hw_timer_frequency(void)
{
    if ((RCC->STGENCKSELR & RCC_STGENCKSELR_STGENSRC) == RCC_STGENCLKSOURCE_HSE)
    {
        return HSE_VALUE;
    }

    return HSI_VALUE;
}
```

RT-Thread 接管 tick 后，HAL 的 `HAL_InitTick()` 不再创建 SysTick：

```c
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RT_UNUSED(TickPriority);
    return HAL_OK;
}
```

PL1 tick handler：

```c
static void rt_hw_tick_handler(void)
{
    rt_uint32_t reload = rt_hw_timer_frequency() / RT_TICK_PER_SECOND;

    IRQ_ClearPending(SecurePhyTimer_IRQn);
    PL1_SetLoadValue(reload + PL1_GetCurrentValue());
    rt_tick_increase();
}
```

tick 初始化：

```c
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
```

board 初始化：

```c
void rt_hw_board_init(void)
{
    HAL_Init();
    BSP_LED_Init();
#ifdef RT_USING_HEAP
    rt_system_heap_init(rt_heap, rt_heap + sizeof(rt_heap));
#endif
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
    rt_hw_tick_init();
}
```

这里的顺序很重要：

1. `HAL_Init()`：HAL 基础初始化。
2. `BSP_LED_Init()`：保留基础板级 LED。
3. `rt_system_heap_init()`：RTT 动态内存。
4. `rt_components_board_init()`：执行 `INIT_BOARD_EXPORT()` 注册的 UART4、存储设备等板级初始化。
5. `rt_hw_tick_init()`：最后开启 RTT tick。

## 10. GIC IRQ 分发

`rt_hw_trap_irq()` 从 GIC 获取当前 active IRQ，查 CMSIS IRQ handler 表并调用：

```c
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
```

所有外设中断如果使用 `IRQ_SetHandler()` 注册，都会从这里进入。此前 eMMC 卡死在该循环的根因就是 `SDMMC2_IRQn` 只 enable，没有注册 handler，level 中断一直 pending。

## 11. 线程初始栈

在 `rtthread_board.c` 中实现：

```c
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
    *(--stk) = 0x13U;

    return (rt_uint8_t *)stk;
}
```

`0x13U` 是 ARM SVC mode。初始 PC 放 `_thread_start`，由 `_thread_start` 用 `BLX R1` 跳到真实线程入口，兼容入口地址可能带 Thumb bit 的情况。

## 12. IAR A7 汇编移植

新增 `User/Core/Src/rtthread_port_iar.s`。

最少必须实现这些符号：

```asm
PUBLIC  rt_hw_interrupt_disable
PUBLIC  rt_hw_interrupt_enable
PUBLIC  rt_hw_context_switch_to
PUBLIC  rt_hw_context_switch
PUBLIC  rt_hw_context_switch_interrupt
PUBLIC  rt_hw_context_switch_exit
PUBLIC  RTThread_IRQ_Handler
PUBLIC  _thread_start
PUBLIC  SWI_Handler
PUBLIC  Undef_Handler
PUBLIC  PAbt_Handler
PUBLIC  DAbt_Handler
PUBLIC  FIQ_Handler
```

关中断：

```asm
rt_hw_interrupt_disable
        MRS     R0, CPSR
        CPSID   i
        BX      LR
```

恢复中断：

```asm
rt_hw_interrupt_enable
        MSR     CPSR_cxsf, R0
        BX      LR
```

首次切换到线程：

```asm
rt_hw_context_switch_to
        CLREX
        MSR     CPSR_c, #0xD3
        LDR     SP, [R0]
        B       rt_hw_context_switch_exit
```

线程上下文切换：

```asm
rt_hw_context_switch
        CLREX
        STMFD   SP!, {LR}
        STMFD   SP!, {R0-R12, LR}
        MRS     R4, CPSR
        TST     LR, #0x01
        ORRNE   R4, R4, #0x20
        STMFD   SP!, {R4}
        STR     SP, [R0]
        LDR     SP, [R1]
        B       rt_hw_context_switch_exit
```

IRQ 入口：

```asm
RTThread_IRQ_Handler
        STMFD   SP!, {R0-R12, LR}

        BL      rt_interrupt_enter
        BL      rt_hw_trap_irq
        BL      rt_interrupt_leave

        LDR     R0, =rt_thread_switch_interrupt_flag
        LDR     R1, [R0]
        CMP     R1, #1
        BEQ     rt_hw_context_switch_interrupt_do

        LDMFD   SP!, {R0-R12, LR}
        SUBS    PC, LR, #4
```

线程启动跳板：

```asm
_thread_start
        MOV     R10, LR
        BLX     R1
        BLX     R10
_thread_halt
        B       _thread_halt
```

异常入口要保存 17 个寄存器槽，传给 C 层异常处理，例如 `DAbt_Handler`：

```asm
DAbt_Handler
        SUB     SP, SP, #68
        STMIA   SP, {R0-R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #60]
        STR     R6, [R0, #64]
        AND     R1, R6, #0x1F
        CMP     R1, #0x10
        BEQ     _dabt_save_usr_sys
        CMP     R1, #0x1F
        BEQ     _dabt_save_usr_sys
        MSR     CPSR_c, #0xD3
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        B       _dabt_call
_dabt_save_usr_sys
        MSR     CPSR_c, #0xDF
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        MSR     CPSR_c, #0xD3
_dabt_call
        BL      rt_hw_trap_dabt
_dabt_halt
        B       _dabt_halt
```

## 13. 异常现场保存

新增 `User/Core/Src/rtthread_trap.c`。

异常现场结构：

```c
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
```

全局保留最后一次异常信息，方便用调试器查看：

```c
volatile struct rt_hw_exp_stack rt_hw_exception_stack;
volatile const char *rt_hw_exception_name;
volatile rt_uint32_t rt_hw_exception_fault_pc;
volatile rt_uint32_t rt_hw_exception_pc_adjust;
```

异常保存函数：

```c
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

    rt_hw_cpu_shutdown();
}
```

PC 修正规则：

```c
void rt_hw_trap_undef(struct rt_hw_exp_stack *regs) { rt_hw_exception_save("undefined instruction", regs, 4U); }
void rt_hw_trap_swi(struct rt_hw_exp_stack *regs)   { rt_hw_exception_save("software interrupt", regs, 4U); }
void rt_hw_trap_pabt(struct rt_hw_exp_stack *regs)  { rt_hw_exception_save("prefetch abort", regs, 4U); }
void rt_hw_trap_dabt(struct rt_hw_exp_stack *regs)  { rt_hw_exception_save("data abort", regs, 8U); }
void rt_hw_trap_resv(struct rt_hw_exp_stack *regs)  { rt_hw_exception_save("reserved", regs, 0U); }
```

Data Abort 的异常 LR 通常需要减 8 才是 fault PC；Undef/SWI/PAbt 当前按减 4 处理。

## 14. UART4 shell 移植

新增 `User/Core/Src/rtthread_shell_port.c`。

全局 UART 句柄和 RTT serial 对象：

```c
UART_HandleTypeDef huart4;
static struct rt_serial_device uart4_serial;
```

UART ops：

```c
static const struct rt_uart_ops uart4_ops =
{
    uart4_configure,
    uart4_control,
    uart4_putc,
    uart4_getc,
    RT_NULL
};
```

最后一个成员当前填 `RT_NULL`，是 DMA transmit 相关回调，当前串口用阻塞发送和中断接收，所以不用。

注册 UART4 为 RTT console：

```c
static int rt_hw_uart4_console_init(void)
{
    rt_err_t result;

    uart4_serial.ops = &uart4_ops;
    uart4_serial.config = (struct serial_configure)RT_SERIAL_CONFIG_DEFAULT;
    uart4_serial.config.bufsz = RT_SERIAL_RB_BUFSZ;

    result = rt_hw_serial_register(&uart4_serial,
                                   RT_CONSOLE_DEVICE_NAME,
                                   RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                                   RT_NULL);
    RT_ASSERT(result == RT_EOK);

    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
    return (int)result;
}
INIT_BOARD_EXPORT(rt_hw_uart4_console_init);
```

`INIT_BOARD_EXPORT()` 会把函数指针放入 `.rti_fn.1` 初始化段。`rt_hw_board_init()` 调用 `rt_components_board_init()` 时会自动执行它。

UART4 MSP 初始化：

```c
void HAL_UART_MspInit(UART_HandleTypeDef *uart_handle)
{
    GPIO_InitTypeDef gpio_init = {0};
    RCC_PeriphCLKInitTypeDef periph_clk_init = {0};

    if (uart_handle->Instance != UART4)
    {
        return;
    }

    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_UART4;
    periph_clk_init.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk_init) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_UART4_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_8;
    gpio_init.Mode = GPIO_MODE_AF;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOD, &gpio_init);

    gpio_init.Pin = GPIO_PIN_6;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOD, &gpio_init);
}
```

串口参数从 RTT serial 框架传入：

```c
static rt_err_t uart4_configure(struct rt_serial_device *serial,
                                struct serial_configure *cfg)
{
    RT_UNUSED(serial);

    huart4.Instance = UART4;
    huart4.Init.BaudRate = cfg->baud_rate;
    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}
```

实际代码中还需要把 `cfg->data_bits`、`cfg->stop_bits`、`cfg->parity` 映射到 HAL 的 `WordLength`、`StopBits`、`Parity`。

使能中断接收：

```c
static rt_err_t uart4_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    rt_ubase_t flag = (rt_ubase_t)arg;

    RT_UNUSED(serial);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_SET_INT:
        if (flag & RT_DEVICE_FLAG_INT_RX)
        {
            __HAL_UART_CLEAR_PEFLAG(&huart4);
            __HAL_UART_CLEAR_FEFLAG(&huart4);
            __HAL_UART_CLEAR_NEFLAG(&huart4);
            __HAL_UART_CLEAR_OREFLAG(&huart4);
            __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
            __HAL_UART_ENABLE_IT(&huart4, UART_IT_ERR);
            __HAL_UART_ENABLE_IT(&huart4, UART_IT_PE);

            IRQ_Disable(UART4_IRQn);
            IRQ_ClearPending(UART4_IRQn);
            IRQ_SetHandler(UART4_IRQn, uart4_irq_handler);
            IRQ_SetPriority(UART4_IRQn, RT_UART4_IRQ_PRIORITY);
            IRQ_SetMode(UART4_IRQn, IRQ_MODE_TRIG_LEVEL);
            IRQ_Enable(UART4_IRQn);
        }
        break;
    }

    return RT_EOK;
}
```

RX IRQ 中不要自己解析命令，只通知 RTT serial 框架：

```c
static void uart4_irq_handler(void)
{
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart4);
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(&huart4);
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(&huart4);
    }
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart4);
    }

    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE) != RESET)
    {
        rt_hw_serial_isr(&uart4_serial, RT_SERIAL_EVENT_RX_IND);
    }
}
```

shell 的命令结束符不是驱动判断的。UART 驱动只按字符进 ringbuffer；FinSH/MSH 在上层根据回车换行判断一条命令结束。

## 15. POSIX 兼容头

DFS/elmfat 会引用一些 POSIX 类型和 errno。当前工程补两个轻量头文件。

`User/Core/Inc/sys/types.h`：

```c
#ifndef USER_SYS_TYPES_H__
#define USER_SYS_TYPES_H__

#include <stddef.h>
#include <stdint.h>

typedef int ssize_t;
typedef int off_t;
typedef int mode_t;
typedef int pid_t;
typedef unsigned int dev_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int useconds_t;
typedef int suseconds_t;

#endif
```

`User/Core/Inc/sys/errno.h`：

```c
#ifndef USER_SYS_ERRNO_H__
#define USER_SYS_ERRNO_H__

#include <errno.h>

#ifndef EPERM
#define EPERM           1
#endif
#ifndef ENOENT
#define ENOENT          2
#endif
#ifndef EIO
#define EIO             5
#endif
#ifndef ENOMEM
#define ENOMEM          12
#endif
#ifndef EINVAL
#define EINVAL          22
#endif
#ifndef ENOSPC
#define ENOSPC          28
#endif
#ifndef ENOSYS
#define ENOSYS          38
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT       110
#endif

#endif
```

当前工程的 `errno.h` 还补了更多 errno 值，按当前文件保留即可。

## 16. 修复 object.c 的 IAR Pe188 警告

IAR 对枚举和整数混用比较敏感。当前在 `Middleware/RT-Thread/src/object.c` 对对象类型做显式转换。

`rt_object_get_information()` 中：

```c
type = (enum rt_object_class_type)(type & ~RT_Object_Class_Static);
```

调用处改成显式 enum：

```c
information = rt_object_get_information((enum rt_object_class_type)type);
```

`rt_object_find()` 中：

```c
if (name == RT_NULL ||
    rt_object_get_information((enum rt_object_class_type)type) == RT_NULL)
    return RT_NULL;
```

这样可以消除 `Warning[Pe188]: enumerated type mixed with another type`。

## 17. W25Q128 BSP

### 17.1 SPI5

新增 `Drivers/BSP/Inc/bsp_spi.h`：

```c
#ifndef BSP_SPI_H__
#define BSP_SPI_H__

#include "main.h"

extern SPI_HandleTypeDef hspi5;

void BSP_SPI5_Init(void);

#endif
```

新增 `Drivers/BSP/Src/bsp_spi.c`。

关键初始化：

```c
SPI_HandleTypeDef hspi5;

void BSP_SPI5_Init(void)
{
    hspi5.Instance = SPI5;
    hspi5.Init.Mode = SPI_MODE_MASTER;
    hspi5.Init.Direction = SPI_DIRECTION_2LINES;
    hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi5.Init.NSS = SPI_NSS_SOFT;
    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;

    if (HAL_SPI_Init(&hspi5) != HAL_OK)
    {
        Error_Handler();
    }
}
```

SPI5 引脚：

```c
/*
 * SPI5 to W25Q128:
 * PH12 MOSI, PH7 SCK, PE4 MISO, PI1 software NSS.
 */
```

### 17.2 W25Q128 读写擦接口

新增 `Drivers/BSP/Inc/bsp_w25q128.h`，核心参数：

```c
#define BSP_W25Q128_FLASH_SIZE                 (16UL * 1024UL * 1024UL)
#define BSP_W25Q128_PAGE_SIZE                  256UL
#define BSP_W25Q128_SECTOR_SIZE                4096UL
#define BSP_W25Q128_JEDEC_MID                  0xEFU
#define BSP_W25Q128_JEDEC_CAPACITY             0x18U
```

对外接口：

```c
bsp_w25q128_status_t BSP_W25Q128_Init(void);
bsp_w25q128_status_t BSP_W25Q128_Reset(void);
bsp_w25q128_status_t BSP_W25Q128_ReadID(bsp_w25q128_id_t *id);
bool BSP_W25Q128_IsPresent(void);
bsp_w25q128_status_t BSP_W25Q128_Read(uint32_t address, uint8_t *buffer, uint32_t length);
bsp_w25q128_status_t BSP_W25Q128_Write(uint32_t address, const uint8_t *buffer, uint32_t length);
bsp_w25q128_status_t BSP_W25Q128_EraseSector(uint32_t address);
bsp_w25q128_status_t BSP_W25Q128_ChipErase(void);
```

注意：NOR flash 写入前必须擦除。当前 block wrapper 在写 512B 逻辑扇区时，会读出 4KB 擦除块、修改缓存、擦除 4KB、写回 4KB。

## 18. eMMC BSP

新增 `Drivers/BSP/Inc/bsp_emmc.h`：

```c
#ifndef BSP_EMMC_H__
#define BSP_EMMC_H__

#include "main.h"

#define BSP_EMMC_PART_USER              0U
#define BSP_EMMC_PART_BOOT1             1U
#define BSP_EMMC_PART_BOOT2             2U
#define BSP_EMMC_PART_RPMB              3U

extern MMC_HandleTypeDef hmmc2;

void BSP_EMMC_Init(void);
HAL_StatusTypeDef BSP_EMMC_GetCardInfo(HAL_MMC_CardInfoTypeDef *card_info);
HAL_StatusTypeDef BSP_EMMC_ReadBlocks(uint32_t lba, uint8_t *buf, uint32_t block_count);
HAL_StatusTypeDef BSP_EMMC_WriteBlocks(uint32_t lba, const uint8_t *buf, uint32_t block_count);
HAL_StatusTypeDef BSP_EMMC_EraseWholeChip(void);
HAL_StatusTypeDef BSP_EMMC_SwitchPart(uint8_t part);
HAL_StatusTypeDef BSP_EMMC_EnableBootPart(uint8_t boot_part);
HAL_StatusTypeDef BSP_EMMC_Read(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef BSP_EMMC_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

#endif
```

`Drivers/BSP/Src/bsp_emmc.c` 中关键点：

```c
MMC_HandleTypeDef hmmc2;
static volatile uint8_t emmc_tx_done = 0U;
static volatile uint8_t emmc_rx_done = 0U;
```

SDMMC2 IRQ handler 必须注册到 GIC：

```c
static void bsp_emmc_irq_handler(void)
{
    HAL_MMC_IRQHandler(&hmmc2);
}
```

在 `HAL_MMC_MspInit()` 中：

```c
IRQ_Disable(SDMMC2_IRQn);
IRQ_ClearPending(SDMMC2_IRQn);
IRQ_SetHandler(SDMMC2_IRQn, bsp_emmc_irq_handler);
IRQ_SetPriority(SDMMC2_IRQn, BSP_EMMC_IRQ_PRIORITY);
IRQ_SetMode(SDMMC2_IRQn, IRQ_MODE_TRIG_LEVEL);
IRQ_Enable(SDMMC2_IRQn);
```

DMA 完成回调：

```c
void HAL_MMC_TxCpltCallback(MMC_HandleTypeDef *hmmc)
{
    if (hmmc == &hmmc2)
    {
        emmc_tx_done = 1U;
    }
}

void HAL_MMC_RxCpltCallback(MMC_HandleTypeDef *hmmc)
{
    if (hmmc == &hmmc2)
    {
        emmc_rx_done = 1U;
    }
}
```

block 读：

```c
HAL_StatusTypeDef BSP_EMMC_ReadBlocks(uint32_t lba, uint8_t *buf, uint32_t block_count)
{
    if ((buf == NULL) || (block_count == 0U))
    {
        return HAL_ERROR;
    }

    if (bsp_emmc_wait_transfer() != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    emmc_rx_done = 0U;
    if (HAL_MMC_ReadBlocks_DMA(&hmmc2, buf, lba, block_count) != HAL_OK)
    {
        return HAL_ERROR;
    }

    while (!emmc_rx_done)
    {
    }

    return bsp_emmc_wait_transfer();
}
```

block 写：

```c
HAL_StatusTypeDef BSP_EMMC_WriteBlocks(uint32_t lba, const uint8_t *buf, uint32_t block_count)
{
    if ((buf == NULL) || (block_count == 0U))
    {
        return HAL_ERROR;
    }

    if (bsp_emmc_wait_transfer() != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    emmc_tx_done = 0U;
    if (HAL_MMC_WriteBlocks_DMA(&hmmc2, (uint8_t *)buf, lba, block_count) != HAL_OK)
    {
        return HAL_ERROR;
    }

    while (!emmc_tx_done)
    {
    }

    return bsp_emmc_wait_transfer();
}
```

当前 eMMC 是整盘 `emmc0`。如果 eMMC user 区前面有固件区，不要对 `emmc0` 执行 `mkfs`，应新增带 LBA 偏移的分区 block device，例如 `emmcfs0`：

```c
real_lba = EMMC_FS_START_LBA + pos;
```

然后只对 `emmcfs0` 格式化和挂载。

## 19. 注册存储 device 和 DFS

新增 `User/Core/Src/rtthread_filesystem_port.c`。

文件开头根据宏选择设备：

```c
#ifdef BSP_USING_EMMC
#include "bsp_emmc.h"
#define EMMC_BLOCK_DEVICE_NAME          "emmc0"
#else
#undef BSP_USING_FS_AUTO_MOUNT_EMMC
#endif

#ifdef BSP_USING_W25Q128
#include "bsp_spi.h"
#include "bsp_w25q128.h"
#define W25Q_MTD_DEVICE_NAME            "w25q"
#else
#undef BSP_USING_W25Q128_BLOCK
#endif

#ifdef BSP_USING_W25Q128_BLOCK
#define W25Q_BLOCK_DEVICE_NAME          "w25q0"
#define W25Q_LOGICAL_SECTOR_SIZE        512U
#define W25Q_ERASE_BLOCK_SIZE           BSP_W25Q128_SECTOR_SIZE
#define W25Q_SECTORS_PER_ERASE_BLOCK    (W25Q_ERASE_BLOCK_SIZE / W25Q_LOGICAL_SECTOR_SIZE)
#else
#undef BSP_USING_FS_AUTO_MOUNT_W25Q
#endif
```

防止两个根文件系统同时自动挂载：

```c
#if defined(BSP_USING_FS_AUTO_MOUNT_EMMC) && defined(BSP_USING_FS_AUTO_MOUNT_W25Q)
#error "Only one root filesystem auto mount device can be enabled"
#endif
```

### 19.1 eMMC block device ops

```c
static rt_ssize_t emmc_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    if (BSP_EMMC_ReadBlocks((uint32_t)pos, (uint8_t *)buffer, (uint32_t)size) != HAL_OK)
    {
        return 0;
    }

    return (rt_ssize_t)size;
}

static rt_ssize_t emmc_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    if (BSP_EMMC_WriteBlocks((uint32_t)pos, (const uint8_t *)buffer, (uint32_t)size) != HAL_OK)
    {
        return 0;
    }

    return (rt_ssize_t)size;
}
```

geometry：

```c
static rt_err_t emmc_control(rt_device_t dev, int cmd, void *args)
{
    HAL_MMC_CardInfoTypeDef card_info;

    RT_UNUSED(dev);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_BLK_GETGEOME:
    {
        struct rt_device_blk_geometry *geometry = (struct rt_device_blk_geometry *)args;

        if (geometry == RT_NULL)
        {
            return -RT_EINVAL;
        }

        if (BSP_EMMC_GetCardInfo(&card_info) != HAL_OK)
        {
            return -RT_EIO;
        }

        geometry->bytes_per_sector = MMC_BLOCKSIZE;
        geometry->sector_count = card_info.LogBlockNbr;
        geometry->block_size = MMC_BLOCKSIZE;
        return RT_EOK;
    }

    case RT_DEVICE_CTRL_BLK_SYNC:
        return RT_EOK;

    default:
        return -RT_ENOSYS;
    }
}
```

注册：

```c
emmc_device.type = RT_Device_Class_Block;
emmc_device.ops = &emmc_ops;
result = rt_device_register(&emmc_device,
                            EMMC_BLOCK_DEVICE_NAME,
                            RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
```

### 19.2 W25Q MTD NOR device

MTD read/write/erase 直接调用 BSP：

```c
static rt_ssize_t w25q_mtd_read(struct rt_mtd_nor_device *device,
                                rt_off_t offset,
                                rt_uint8_t *data,
                                rt_size_t length)
{
    RT_UNUSED(device);

    if ((data == RT_NULL) || (length == 0U))
    {
        return 0;
    }

    if ((offset < 0) || (((rt_uint32_t)offset + length) > BSP_W25Q128_FLASH_SIZE))
    {
        return -RT_EINVAL;
    }

    if (w25q_lock_take() != RT_EOK)
    {
        return -RT_EBUSY;
    }

    if (BSP_W25Q128_Read((rt_uint32_t)offset, data, (rt_uint32_t)length) != BSP_W25Q128_OK)
    {
        w25q_lock_release();
        return -RT_EIO;
    }

    w25q_lock_release();
    return (rt_ssize_t)length;
}
```

注册：

```c
w25q_mtd_device.block_size = BSP_W25Q128_SECTOR_SIZE;
w25q_mtd_device.block_start = 0U;
w25q_mtd_device.block_end = BSP_W25Q128_SECTOR_COUNT;
w25q_mtd_device.ops = &w25q_mtd_ops;

result = rt_mtd_nor_register_device(W25Q_MTD_DEVICE_NAME, &w25q_mtd_device);
```

### 19.3 W25Q block device wrapper

FAT 需要 block device，W25Q 原生是 MTD NOR，所以包装成 `w25q0`：

```c
static rt_ssize_t w25q_block_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_uint32_t address;
    rt_uint32_t length;

    RT_UNUSED(dev);

    if ((buffer == RT_NULL) || (size == 0U))
    {
        return 0;
    }

    address = (rt_uint32_t)pos * W25Q_LOGICAL_SECTOR_SIZE;
    length = (rt_uint32_t)size * W25Q_LOGICAL_SECTOR_SIZE;

    if ((address + length) > BSP_W25Q128_FLASH_SIZE)
    {
        return 0;
    }

    if (w25q_mtd_read(&w25q_mtd_device, address, (rt_uint8_t *)buffer, length) != (rt_ssize_t)length)
    {
        return 0;
    }

    return (rt_ssize_t)size;
}
```

写入 512B 扇区时按 4KB 擦除块读改写：

```c
static rt_err_t w25q_block_write_one_erase_block(rt_uint32_t erase_block_index,
                                                 rt_uint32_t start_sector_in_block,
                                                 const rt_uint8_t *buffer,
                                                 rt_uint32_t sector_count)
{
    rt_uint32_t erase_address = erase_block_index * W25Q_ERASE_BLOCK_SIZE;
    rt_uint32_t sector_offset = start_sector_in_block * W25Q_LOGICAL_SECTOR_SIZE;
    rt_uint32_t length = sector_count * W25Q_LOGICAL_SECTOR_SIZE;

    if (BSP_W25Q128_Read(erase_address, w25q_cache, W25Q_ERASE_BLOCK_SIZE) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    memcpy(&w25q_cache[sector_offset], buffer, length);

    if (BSP_W25Q128_EraseSector(erase_address) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    if (BSP_W25Q128_Write(erase_address, w25q_cache, W25Q_ERASE_BLOCK_SIZE) != BSP_W25Q128_OK)
    {
        return -RT_EIO;
    }

    return RT_EOK;
}
```

geometry：

```c
geometry->bytes_per_sector = W25Q_LOGICAL_SECTOR_SIZE;
geometry->sector_count = BSP_W25Q128_FLASH_SIZE / W25Q_LOGICAL_SECTOR_SIZE;
geometry->block_size = W25Q_ERASE_BLOCK_SIZE;
```

`w25q0` 的容量为：

```text
16 MB / 512 B = 32768 sectors
```

`df` 显示的可用空间会小于 16 MB，例如 `15.9 MB`，因为 FAT 本身会占用引导扇区、FAT 表、根目录等元数据空间。

### 19.4 自动初始化和挂载

存储设备注册函数：

```c
static int rt_hw_storage_device_init(void)
{
    BSP_EMMC_Init();
    rt_device_register(&emmc_device, "emmc0", RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);

    BSP_SPI5_Init();
    (void)BSP_W25Q128_Init();
    (void)BSP_W25Q128_Reset();
    rt_mtd_nor_register_device("w25q", &w25q_mtd_device);
    rt_device_register(&w25q_block_device, "w25q0",
                       RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);

    return 0;
}
INIT_BOARD_EXPORT(rt_hw_storage_device_init);
```

实际代码用 `#ifdef BSP_USING_EMMC`、`#ifdef BSP_USING_W25Q128`、`#ifdef BSP_USING_W25Q128_BLOCK` 做条件编译。

自动挂载函数：

```c
static int rt_hw_filesystem_mount(void)
{
#if defined(BSP_USING_EMMC) && defined(BSP_USING_FS_AUTO_MOUNT_EMMC)
    if (dfs_mount(EMMC_BLOCK_DEVICE_NAME, "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("emmc mounted on /\n");
    }
    else
    {
        rt_kprintf("emmc mount failed, run: mkfs -t elm %s\n", EMMC_BLOCK_DEVICE_NAME);
    }
#elif defined(BSP_USING_W25Q128_BLOCK) && defined(BSP_USING_FS_AUTO_MOUNT_W25Q)
    if (dfs_mount(W25Q_BLOCK_DEVICE_NAME, "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("w25q mounted on /\n");
    }
    else
    {
        rt_kprintf("w25q mount failed, run: mkfs -t elm %s\n", W25Q_BLOCK_DEVICE_NAME);
    }
#endif

    return 0;
}
INIT_APP_EXPORT(rt_hw_filesystem_mount);
```

挂载放在 `INIT_APP_EXPORT()`，是为了等 DFS、FinSH 等组件初始化完成后再执行。

## 20. stm32mp13xx_it.c

当前 `User/Core/Src/stm32mp13xx_it.c` 基本保留 HAL 模板。eMMC 的真正中断处理不在这里写 `SDMMC2_IRQHandler()`，而是在 `bsp_emmc.c` 中通过：

```c
IRQ_SetHandler(SDMMC2_IRQn, bsp_emmc_irq_handler);
```

让 `rt_hw_trap_irq()` 分发到 HAL：

```c
HAL_MMC_IRQHandler(&hmmc2);
```

因此 `stm32mp13xx_it.c` 只需要保证没有定义冲突即可。

## 21. lwIP / ETH1 网络移植

本工程使用 RT-Thread 自带 lwIP 2.1.2，不修改 RTT lwIP 源码本体。板级相关代码只放在 `Drivers/BSP`：

```text
Drivers/BSP/Inc/bsp_eth.h
Drivers/BSP/Src/bsp_eth.c
Drivers/BSP/Inc/bsp_yt8531c.h
Drivers/BSP/Src/bsp_yt8531c.c
```

RTT lwIP 负责协议栈、`tcpip_thread`、`ethernetif` 线程和 `ping` 命令；`bsp_eth.c` 只负责 ETH1 HAL、DMA descriptor、PHY、cache、IRQ 和 `struct eth_device` 注册。

### 21.1 ETH 头文件

`Drivers/BSP/Inc/bsp_eth.h` 必须包含 `rtconfig.h`，否则 `bsp_eth.c` / `bsp_yt8531c.c` 里的 `BSP_USING_ETH` 可能不可见，导致 ETH 源码实际没有编译进去：

```c
#ifndef BSP_ETH_H__
#define BSP_ETH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rtconfig.h"
#include "main.h"

extern ETH_HandleTypeDef heth1;
extern ETH_TxPacketConfigTypeDef TxConfig;

int BSP_ETH_Init(void);

#ifdef __cplusplus
}
#endif

#endif
```

`Drivers/BSP/Src/bsp_yt8531c.c` 同样要包含：

```c
#include "bsp_yt8531c.h"
#include "rtconfig.h"

#ifdef BSP_USING_ETH
```

### 21.2 ETH 全局对象和 RX pbuf pool

`Drivers/BSP/Src/bsp_eth.c` 文件开头：

```c
#include "bsp_eth.h"

#ifdef BSP_USING_ETH

#include "bsp_yt8531c.h"
#include "cachel1_armv7.h"
#include "irq_ctrl.h"
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/ethernetif.h>
#include <stddef.h>
#include <string.h>
```

设备名、MAC、buffer：

```c
#define BSP_ETH_DEVICE_NAME             "e0"
#define BSP_ETH_MAC_ADDR0               0x00U
#define BSP_ETH_MAC_ADDR1               0x80U
#define BSP_ETH_MAC_ADDR2               0xE1U
#define BSP_ETH_MAC_ADDR3               0x00U
#define BSP_ETH_MAC_ADDR4               0x00U
#define BSP_ETH_MAC_ADDR5               0x00U

#define BSP_ETH_TX_BUFFER_SIZE          1536U
#define BSP_ETH_RX_BUFFER_SIZE          1536U
#define BSP_ETH_RX_BUFFER_COUNT         12U
```

RX 使用 lwIP custom pbuf 包装 DMA buffer，所以 `rtconfig.h` 必须定义 `LWIP_SUPPORT_CUSTOM_PBUF 1`：

```c
typedef struct
{
    struct pbuf_custom pbuf_custom;
    rt_uint8_t buff[(BSP_ETH_RX_BUFFER_SIZE + 31U) & ~31U] __attribute__((aligned(32)));
} bsp_eth_rx_buff_t;

ETH_HandleTypeDef heth1;
ETH_TxPacketConfigTypeDef TxConfig;

static ETH_DMADescTypeDef eth_rx_desc[ETH_RX_DESC_CNT] __attribute__((aligned(32)));
static ETH_DMADescTypeDef eth_tx_desc[ETH_TX_DESC_CNT] __attribute__((aligned(32)));
static rt_uint8_t eth_tx_buffer[BSP_ETH_TX_BUFFER_SIZE] __attribute__((aligned(32)));
static struct eth_device eth_device;
static yt8531c_Object_t yt8531c;

LWIP_MEMPOOL_DECLARE(ETH_RX_POOL,
                     BSP_ETH_RX_BUFFER_COUNT,
                     sizeof(bsp_eth_rx_buff_t),
                     "ETH RX PBUF pool");
```

### 21.3 cache 维护

Cortex-A7 开启 cache 后，ETH DMA buffer 必须做 cache clean/invalidate。地址按 32 字节 cache line 向下/向上取整：

```c
#ifdef CACHE_USE
static void bsp_eth_cache_clean(const void *addr, rt_size_t size)
{
    rt_ubase_t start;
    rt_ubase_t end;

    if ((addr == RT_NULL) || (size == 0U))
    {
        return;
    }

    start = ((rt_ubase_t)addr) & ~(rt_ubase_t)31U;
    end = (((rt_ubase_t)addr) + size + 31U) & ~(rt_ubase_t)31U;
    SCB_CleanDCache_by_Addr((void *)start, (int32_t)(end - start));
}

static void bsp_eth_cache_invalidate(const void *addr, rt_size_t size)
{
    rt_ubase_t start;
    rt_ubase_t end;

    if ((addr == RT_NULL) || (size == 0U))
    {
        return;
    }

    start = ((rt_ubase_t)addr) & ~(rt_ubase_t)31U;
    end = (((rt_ubase_t)addr) + size + 31U) & ~(rt_ubase_t)31U;
    SCB_InvalidateDCache_by_Addr((void *)start, (int32_t)(end - start));
}
#else
#define bsp_eth_cache_clean(addr, size)       do { (void)(addr); (void)(size); } while (0)
#define bsp_eth_cache_invalidate(addr, size)  do { (void)(addr); (void)(size); } while (0)
#endif
```

TX 拷贝到 `eth_tx_buffer` 后调用 `bsp_eth_cache_clean()`，RX 回调里在交给 lwIP 前调用 `bsp_eth_cache_invalidate()`。

### 21.4 PHY 初始化和 link 线程

`bsp_eth.c` 通过 HAL MDIO 读写函数注册给 YT8531C 驱动：

```c
static rt_err_t bsp_eth_phy_init(void)
{
    yt8531c_IOCtx_t ioctx;

    memset(&ioctx, 0, sizeof(ioctx));
    ioctx.Init = bsp_eth_phy_io_init;
    ioctx.DeInit = bsp_eth_phy_io_deinit;
    ioctx.ReadReg = bsp_eth_phy_io_read_reg;
    ioctx.WriteReg = bsp_eth_phy_io_write_reg;
    ioctx.GetTick = bsp_eth_phy_io_get_tick;

    if (YT8531C_RegisterBusIO(&yt8531c, &ioctx) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_Init(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_XtalInit() != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_ConfigRGMII_Delay(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }

    YT8531C_LED_Init();

    if (YT8531C_DisablePowerDownMode(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }
    if (YT8531C_StartAutoNego(&yt8531c) != YT8531C_STATUS_OK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}
```

link 线程每 1 秒读取 PHY 状态。link up 时根据速率/双工配置 MAC、启动 `HAL_ETH_Start_IT()`，并通知 RTT lwIP：

```c
static void bsp_eth_update_link(void)
{
    int32_t link_state;

    link_state = YT8531C_GetLinkState(&yt8531c);
    if (bsp_eth_is_valid_link_state(link_state))
    {
        if ((eth_started == RT_FALSE) || (eth_applied_link_state != link_state))
        {
            (void)HAL_ETH_Stop_IT(&heth1);
            if (bsp_eth_apply_mac_config(link_state) == HAL_OK)
            {
                if (HAL_ETH_Start_IT(&heth1) == HAL_OK)
                {
                    eth_started = RT_TRUE;
                    (void)eth_device_linkchange(&eth_device, RT_TRUE);
                    rt_kprintf("eth link up: %d\n", link_state);
                }
            }
        }
    }
    else
    {
        if (eth_started == RT_TRUE)
        {
            (void)HAL_ETH_Stop_IT(&heth1);
            eth_started = RT_FALSE;
            (void)eth_device_linkchange(&eth_device, RT_FALSE);
            rt_kprintf("eth link down\n");
        }
        eth_applied_link_state = YT8531C_STATUS_LINK_DOWN;
    }
}
```

### 21.5 RTT eth_device 注册

`BSP_ETH_Init()` 完成 HAL ETH 初始化、TX 配置和 PHY 初始化：

```c
int BSP_ETH_Init(void)
{
    memset(&heth1, 0, sizeof(heth1));

    heth1.Instance = ETH;
    heth1.Init.MACAddr = eth_mac_addr;
    heth1.Init.MediaInterface = HAL_ETH_RGMII_MODE;
    heth1.Init.TxDesc = eth_tx_desc;
    heth1.Init.RxDesc = eth_rx_desc;
    heth1.Init.RxBuffLen = BSP_ETH_RX_BUFFER_SIZE;

    RCC->MP_AHB6ENSETR = RCC_MP_AHB6ENSETR_ETH1MACEN |
                         RCC_MP_AHB6ENSETR_ETH1RXEN |
                         RCC_MP_AHB6ENSETR_ETH1TXEN |
                         RCC_MP_AHB6ENSETR_ETH1CKEN;

    if (HAL_ETH_Init(&heth1) != HAL_OK)
    {
        return -RT_ERROR;
    }

    memset(&TxConfig, 0, sizeof(TxConfig));
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    return (bsp_eth_phy_init() == RT_EOK) ? 0 : -RT_ERROR;
}
```

`rt_hw_eth_init()` 注册为 device 初始化阶段：

```c
static int rt_hw_eth_init(void)
{
    rt_err_t result;

    LWIP_MEMPOOL_INIT(ETH_RX_POOL);

    if (BSP_ETH_Init() != 0)
    {
        rt_kprintf("eth hardware init failed\n");
        return -RT_ERROR;
    }

    memset(&eth_device, 0, sizeof(eth_device));
#ifdef RT_USING_DEVICE_OPS
    eth_device.parent.ops = &bsp_eth_ops;
#else
    eth_device.parent.init = bsp_eth_dev_init;
    eth_device.parent.open = bsp_eth_dev_open;
    eth_device.parent.close = bsp_eth_dev_close;
    eth_device.parent.read = bsp_eth_dev_read;
    eth_device.parent.write = bsp_eth_dev_write;
    eth_device.parent.control = bsp_eth_dev_control;
#endif
    eth_device.eth_rx = bsp_eth_rx;
    eth_device.eth_tx = bsp_eth_tx;

    result = eth_device_init(&eth_device, BSP_ETH_DEVICE_NAME);
    if (result != RT_EOK)
    {
        rt_kprintf("eth device register failed: %d\n", result);
        return result;
    }

    eth_link_thread = rt_thread_create("ethlink",
                                       bsp_eth_link_thread_entry,
                                       RT_NULL,
                                       1024,
                                       RT_LWIP_ETHTHREAD_PRIORITY,
                                       20);
    if (eth_link_thread != RT_NULL)
    {
        rt_thread_startup(eth_link_thread);
    }

    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_hw_eth_init);
```

这里用 `INIT_DEVICE_EXPORT()`，因为它必须在 RTT board 初始化之后、应用层之前注册网卡设备。lwIP 的 `ethernetif` 后续会发现并接管这个 `eth_device`。

### 21.6 ETH IRQ 和 HAL 回调

在 `HAL_ETH_MspInit()` 中注册 GIC handler：

```c
IRQ_Disable(ETH1_IRQn);
IRQ_ClearPending(ETH1_IRQn);
IRQ_SetHandler(ETH1_IRQn, bsp_eth_irq_handler);
IRQ_SetPriority(ETH1_IRQn, BSP_ETH_IRQ_PRIORITY);
IRQ_SetMode(ETH1_IRQn, IRQ_MODE_TRIG_LEVEL | IRQ_MODE_TYPE_IRQ | IRQ_MODE_CPU_0);
IRQ_Enable(ETH1_IRQn);
```

handler 调 HAL：

```c
static void bsp_eth_irq_handler(void)
{
    HAL_ETH_IRQHandler(&heth1);
}
```

RX 完成后只通知 RTT `ethernetif` 线程，不在中断里直接跑协议栈：

```c
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    RT_UNUSED(heth);
    (void)eth_device_ready(&eth_device);
}
```

### 21.7 RX custom pbuf 和 `ETH_PAD_SIZE=2`

`RT_LWIP_ETH_PAD_SIZE` 在 RTT 的 `Middleware/RT-Thread/components/net/lwip/port/lwipopts.h` 中映射成 lwIP `ETH_PAD_SIZE`：

```c
#ifdef RT_LWIP_ETH_PAD_SIZE
#define ETH_PAD_SIZE                RT_LWIP_ETH_PAD_SIZE
#endif
```

Cortex-A7 上如果 IP 头不按 4 字节对齐，可能在 lwIP 解析 IP/ICMP/TCP/UDP 头时触发 `data abort exception`。以太网头是 14 字节，所以需要在帧前保留 2 字节 pad。

RX allocate 回调给 HAL 的 buffer 指向 `buffer + ETH_PAD_SIZE`：

```c
void HAL_ETH_RxAllocateCallback(ETH_HandleTypeDef *heth, uint8_t **buff)
{
    struct pbuf_custom *custom_pbuf;
    rt_uint8_t *buffer;

    RT_UNUSED(heth);

    custom_pbuf = LWIP_MEMPOOL_ALLOC(ETH_RX_POOL);
    if (custom_pbuf == RT_NULL)
    {
        eth_rx_alloc_status = BSP_ETH_RX_ALLOC_ERROR;
        *buff = RT_NULL;
        return;
    }

    buffer = (rt_uint8_t *)custom_pbuf + offsetof(bsp_eth_rx_buff_t, buff);
    *buff = buffer + ETH_PAD_SIZE;

    custom_pbuf->custom_free_function = bsp_eth_pbuf_free_custom;
    (void)pbuf_alloced_custom(PBUF_RAW,
                              0,
                              PBUF_REF,
                              custom_pbuf,
                              buffer,
                              BSP_ETH_RX_BUFFER_SIZE - ETH_PAD_SIZE);
}
```

HAL 收到帧后，把 pbuf 指针从 DMA buffer 反算回来，并把长度加回 pad：

```c
void HAL_ETH_RxLinkCallback(ETH_HandleTypeDef *heth,
                            void **p_start,
                            void **p_end,
                            uint8_t *buff,
                            uint16_t length)
{
    struct pbuf **pp_start = (struct pbuf **)p_start;
    struct pbuf **pp_end = (struct pbuf **)p_end;
    struct pbuf *p;

    RT_UNUSED(heth);

    if ((buff == RT_NULL) || (length == 0U))
    {
        return;
    }

    bsp_eth_cache_invalidate(buff, length);

    p = (struct pbuf *)(buff - ETH_PAD_SIZE - offsetof(bsp_eth_rx_buff_t, buff));
    p->next = RT_NULL;
    p->tot_len = length + ETH_PAD_SIZE;
    p->len = length + ETH_PAD_SIZE;

    if (*pp_start == RT_NULL)
    {
        *pp_start = p;
    }
    else
    {
        (*pp_end)->next = p;
    }

    *pp_end = p;
}
```

lwIP 后续会按 `ETH_PAD_SIZE` 去掉 pad，最终 IP 头落在 4 字节对齐地址。

### 21.8 TX 去掉 pad 再发

lwIP 发出来的 pbuf 前面也带 2 字节 pad，真正送给 ETH MAC 的帧必须从以太网头开始。因此 TX 路径先 `pbuf_remove_header()`，发完再恢复：

```c
static rt_err_t bsp_eth_tx(rt_device_t dev, struct pbuf *p)
{
    struct pbuf *q;
    rt_uint32_t frame_len = 0;
    ETH_BufferTypeDef tx_buffer;
    rt_err_t result = RT_EOK;

    RT_UNUSED(dev);

    if ((p == RT_NULL) || (p->tot_len > BSP_ETH_TX_BUFFER_SIZE) || (eth_started == RT_FALSE))
    {
        return -RT_ERROR;
    }

#if ETH_PAD_SIZE
    if (pbuf_remove_header(p, ETH_PAD_SIZE) != 0)
    {
        return -RT_ERROR;
    }
#endif

    if (p->tot_len > BSP_ETH_TX_BUFFER_SIZE)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    for (q = p; q != RT_NULL; q = q->next)
    {
        memcpy(&eth_tx_buffer[frame_len], q->payload, q->len);
        frame_len += q->len;
    }

    bsp_eth_cache_clean(eth_tx_buffer, frame_len);

    memset(&tx_buffer, 0, sizeof(tx_buffer));
    tx_buffer.buffer = eth_tx_buffer;
    tx_buffer.len = frame_len;
    tx_buffer.next = RT_NULL;

    TxConfig.Length = frame_len;
    TxConfig.TxBuffer = &tx_buffer;
    TxConfig.pData = RT_NULL;

    if (HAL_ETH_Transmit(&heth1, &TxConfig, 100U) != HAL_OK)
    {
        result = -RT_ERROR;
    }

__exit:
#if ETH_PAD_SIZE
    (void)pbuf_add_header(p, ETH_PAD_SIZE);
#endif

    return result;
}
```

如果只做 RX pad，不做 TX remove，网卡会把 pad 当成以太网帧前 2 字节内容发出去，抓包会异常。

### 21.9 静态 IP 和 DHCP 切换

`rtconfig.h` 中用 `BSP_ETH_IP_MODE` 选择：

```c
#define BSP_ETH_IP_MODE_STATIC          0
#define BSP_ETH_IP_MODE_DHCP            1
#define BSP_ETH_IP_MODE                 BSP_ETH_IP_MODE_STATIC

#if (BSP_ETH_IP_MODE == BSP_ETH_IP_MODE_DHCP)
#define RT_LWIP_DHCP
#elif (BSP_ETH_IP_MODE == BSP_ETH_IP_MODE_STATIC)
#undef RT_LWIP_DHCP
#else
#error "Invalid BSP_ETH_IP_MODE"
#endif

#define RT_LWIP_IPADDR                 "192.168.6.6"
#define RT_LWIP_GWADDR                 "192.168.6.1"
#define RT_LWIP_MSKADDR                "255.255.255.0"
```

静态模式下，电脑可以设置同网段地址，例如 `192.168.6.60/255.255.255.0`，然后互相 ping：

```text
ping 192.168.6.6
```

板子 ping 电脑时，如果电脑能抓到 request 但没有 reply，通常是 Windows 防火墙禁止入站 ICMPv4。可以先临时关闭防火墙验证，或添加 ICMPv4 Echo Request 入站允许规则。

### 21.10 iperf 测速命令

lwIP 2.1.2 自带 `lwiperf`，源码为：

```text
Middleware/RT-Thread/components/net/lwip/lwip-2.1.2/src/apps/lwiperf/lwiperf.c
```

当前工程新增 `User/Core/Src/rtthread_iperf_port.c`，导出 MSH 命令 `iperf`。`lwiperf` 使用 raw TCP API，所以 shell 命令不直接调用 `lwiperf_start_*()`，而是通过 `tcpip_callback()` 投递到 lwIP `tcpip_thread` 中执行：

```c
err = tcpip_callback(callback, ctx);
```

支持命令：

```text
iperf -s [port]
iperf -c <ip> [port]
iperf stop
```

板子作为 server：

```text
msh />iperf -s
```

电脑端使用 iperf2 连接板子：

```powershell
iperf -c 192.168.6.6 -p 5001
```

板子作为 client：

```text
msh />iperf -c 192.168.6.60
```

电脑端先启动 iperf2 server：

```powershell
iperf -s -p 5001
```

注意：这里兼容的是 iperf2 协议，不是 iperf3。电脑端不要用 `iperf3`。

## 22. device 和 component 的关系

component 是初始化顺序机制，device 是统一设备模型。

以 UART4 为例：

1. `rt_hw_uart4_console_init()` 用 `INIT_BOARD_EXPORT()` 放入自动初始化段。
2. `rt_hw_board_init()` 调用 `rt_components_board_init()`。
3. `rt_components_board_init()` 执行 `rt_hw_uart4_console_init()`。
4. `rt_hw_uart4_console_init()` 调用 `rt_hw_serial_register()`。
5. `rt_hw_serial_register()` 内部注册 RTT device。
6. shell 里 `list device` 可以看到 `uart4`。

以 W25Q 为例：

1. `rt_hw_storage_device_init()` 用 `INIT_BOARD_EXPORT()` 注册。
2. 启动时初始化 SPI5、W25Q128。
3. `rt_mtd_nor_register_device("w25q", &w25q_mtd_device)` 注册 MTD。
4. `rt_device_register(&w25q_block_device, "w25q0", ...)` 注册 block device。
5. DFS/elmfat 只挂载 block device `w25q0`，不直接挂载 MTD device `w25q`。

以 ETH1 为例：

1. `rt_hw_eth_init()` 用 `INIT_DEVICE_EXPORT()` 注册。
2. 启动时初始化 ETH1 HAL、DMA descriptor、YT8531C PHY。
3. `eth_device_init(&eth_device, "e0")` 注册 RTT lwIP 网卡设备。
4. `eth_device.eth_rx = bsp_eth_rx`，`eth_device.eth_tx = bsp_eth_tx` 对接收发函数。
5. RX 中断只调用 `eth_device_ready()` 唤醒 RTT `ethernetif` 线程。
6. link 变化时调用 `eth_device_linkchange()` 通知 lwIP netif up/down。

## 23. 常用 shell 指令

查看对象：

```text
list thread
list sem
list mutex
list device
```

W25Q 格式化、挂载、读写：

```text
mkfs -t elm w25q0
mount w25q0 / elm
echo "hello" /hello.txt
cat /hello.txt
df
```

eMMC 格式化、挂载、读写：

```text
mkfs -t elm emmc0
mount emmc0 / elm
echo "hello" /hello.txt
cat /hello.txt
df
```

如果 eMMC user 区开头存固件，不要直接格式化 `emmc0`。应先做偏移分区设备 `emmcfs0`，再：

```text
mkfs -t elm emmcfs0
mount emmcfs0 / elm
```

当前 MSH 的 `echo` 语法不是 shell 重定向：

```text
echo "string" filename
```

正确例子：

```text
echo "hello" /hello.txt
```

不是：

```text
echo "hello" > /hello.txt
```

网络：

```text
list device
ping 192.168.6.60
iperf -s
iperf -c 192.168.6.60
iperf stop
```

当前静态 IP 默认是 `192.168.6.6`。如果电脑设置为 `192.168.6.60/255.255.255.0`，电脑 ping 板子：

```text
ping 192.168.6.6
```

## 24. 文件系统代码中读写文件

在应用线程中可以用 DFS POSIX 接口：

```c
#include <dfs_posix.h>

void fs_demo(void)
{
    int fd;
    char buf[32];
    int len;

    fd = open("/hello.txt", O_CREAT | O_RDWR | O_TRUNC, 0);
    if (fd >= 0)
    {
        write(fd, "hello", 5);
        close(fd);
    }

    fd = open("/hello.txt", O_RDONLY, 0);
    if (fd >= 0)
    {
        len = read(fd, buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            rt_kprintf("%s\n", buf);
        }
        close(fd);
    }
}
```

也可以直接用 DFS API，例如 `dfs_mount()`、`dfs_unmount()`。

## 25. 构建验证

命令行构建：

```powershell
iarbuild C:\Users\Kamisato\Desktop\folder\test\EWARM\MP135.ewp -build Debug
iarbuild C:\Users\Kamisato\Desktop\folder\test\EWARM\MP135.ewp -build Release
```

启动后串口应看到：

```text
 \ | /
- RT -     Thread Operating System
 / | \     5.2.2 build ...
 2006 - 2024 Copyright by RT-Thread team
```

这个 logo 由 `rtthread_startup()` 中的 `rt_show_version()` 打印，不属于某个用户线程；它发生在 scheduler 启动之前。

然后 shell 出现：

```text
msh />
```

执行：

```text
list device
```

当前配置正常时应看到类似：

```text
uart4    Character Device
emmc0    Block Device
w25q0    Block Device
w25q     MTD Device
e0       Network Interface
```

实际显示取决于 `BSP_USING_*` 宏和板上器件是否初始化成功。

网络验证：

```text
ping 192.168.6.60
```

或者在电脑侧 ping 板子：

```text
ping 192.168.6.6
```

## 26. 常见问题

### 26.1 不进 rt_hw_tick_handler

检查：

```c
IRQ_SetHandler(SecurePhyTimer_IRQn, rt_hw_tick_handler);
IRQ_SetMode(SecurePhyTimer_IRQn, IRQ_MODE_TRIG_EDGE);
IRQ_Enable(SecurePhyTimer_IRQn);
PL1_SetControl(0x1U);
```

还要确认启动文件 IRQ 向量已经指向：

```asm
LDR    PC, =RTThread_IRQ_Handler
```

### 26.2 卡在 rt_hw_trap_irq 出不去

通常是 level IRQ pending 但没有 handler 清中断源。典型例子是 SDMMC2：

```c
IRQ_SetHandler(SDMMC2_IRQn, bsp_emmc_irq_handler);
```

handler 中必须调用：

```c
HAL_MMC_IRQHandler(&hmmc2);
```

否则 GIC end interrupt 后外设中断源仍然 pending，会立刻再次进入。

### 26.3 重启后文件丢失

常见原因：

- 上次只写到 RAM 缓存，未 sync/close。
- 没有挂载同一个设备。
- 启动自动挂载失败，实际根目录不是目标 FAT。
- 对 W25Q 的 block wrapper 写回逻辑或擦除块缓存异常。

写文件后至少确保 `close(fd)`，shell 命令写完通常会 close。

### 26.4 W25Q df 只有 15.9 MB

W25Q128 物理容量是 16 MiB：

```text
16 * 1024 * 1024 = 16777216 bytes
```

`w25q0` 按 512B 扇区暴露：

```text
16777216 / 512 = 32768 sectors
```

FAT 格式化后，文件系统元数据占用一部分空间，所以 `df` 显示可用空间小于 16 MiB。

### 26.5 ETH 收包 data abort

现象类似：

```text
data abort exception
fault pc: 0xc000c26e, exception lr/pc: 0xc000c276
```

如果异常发生在 lwIP 处理 ICMP/IP 包附近，优先检查 `ETH_PAD_SIZE`。以太网头是 14 字节，不加 pad 时 IP 头不是 4 字节对齐，Cortex-A7 上可能触发非对齐访问异常。

当前修复点：

```c
#define RT_LWIP_ETH_PAD_SIZE           2
#define LWIP_SUPPORT_CUSTOM_PBUF       1
```

RX：

```c
*buff = buffer + ETH_PAD_SIZE;
p->tot_len = length + ETH_PAD_SIZE;
p->len = length + ETH_PAD_SIZE;
```

TX：

```c
pbuf_remove_header(p, ETH_PAD_SIZE);
/* copy payload to DMA TX buffer and transmit */
pbuf_add_header(p, ETH_PAD_SIZE);
```

### 26.6 电脑 ping 板子通，板子 ping 电脑不通

如果抓包能看到：

```text
192.168.6.6 -> 192.168.6.60 ICMP Echo request
```

但没有：

```text
192.168.6.60 -> 192.168.6.6 ICMP Echo reply
```

说明板子 TX 已经正常，电脑收到了板子的 ICMP request。常见原因是 Windows 防火墙禁止入站 ICMPv4 Echo Request。电脑主动 ping 板子能通，不代表电脑允许被 ping。

可用管理员 PowerShell 添加规则：

```powershell
netsh advfirewall firewall add rule name="Allow ICMPv4 Echo Request" protocol=icmpv4:8,any dir=in action=allow
```

### 26.7 固件区是否纳入文件系统

不建议纳入。推荐布局：

```text
eMMC user area
+----------------------+---------------------------+
| firmware raw area    | DFS / FAT area            |
| LBA 0 ~ N-1          | LBA N ~ end               |
+----------------------+---------------------------+
```

实现方式是在 `emmc0` 上再包装一个偏移 block device：

```c
#define EMMC_FS_START_LBA  固件区结束后的 LBA

static rt_ssize_t emmcfs_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return emmc_read(dev, EMMC_FS_START_LBA + pos, buffer, size);
}
```

然后只对新设备 `emmcfs0` 执行 `mkfs`。

## 27. 移植完成检查表

1. `rtconfig.h` 宏和当前目标一致。
2. IAR include path 包含 RTT include、drivers include、finsh、dfs_v1、elmfat、libc common、lwIP port、lwIP include。
3. IAR 工程加入所有 RTT kernel、device、finsh、dfs、lwIP 源文件。
4. 启动文件向量表从 FreeRTOS IRQ/SWI 改为 RTT handler。
5. `rtthread_port_iar.s` 中实现上下文切换、IRQ、异常入口。
6. `rtthread_board.c` 中初始化 heap、component board、PL1 tick。
7. UART4 注册为 RTT serial device，RX 中断调用 `rt_hw_serial_isr()`。
8. eMMC/W25Q 初始化函数通过 `INIT_BOARD_EXPORT()` 注册。
9. 文件系统自动挂载通过 `INIT_APP_EXPORT()` 注册。
10. ETH1 通过 `INIT_DEVICE_EXPORT()` 注册为 RTT lwIP 网卡设备 `e0`。
11. ETH RX/TX buffer 做 32 字节对齐，cache 模式下 TX clean、RX invalidate。
12. `RT_LWIP_ETH_PAD_SIZE` 为 `2`，RX/TX 都正确处理 pad。
13. `iperf` MSH 命令使用 `tcpip_callback()` 在 lwIP `tcpip_thread` 中启动/停止 `lwiperf`。
14. `list device` 能看到目标设备。
15. `mkfs`、`mount`、`echo`、`cat`、`df` 能正常运行。
16. `ping 192.168.6.60` 或电脑 ping `192.168.6.6` 能正常收发。
17. `iperf -s` 和 `iperf -c 192.168.6.60` 能配合电脑 iperf2 测试 TCP 吞吐。
18. eMMC 如有固件区，禁止直接 `mkfs emmc0`，先做分区偏移设备。
