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
#define RT_LWIP_USING_HW_CHECKSUM
#define LWIP_NO_TX_THREAD

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
#define RT_LWIP_PBUF_NUM               32
/* RAW PCB 数量。 */
#define RT_LWIP_RAW_PCB_NUM            4
/* UDP PCB 数量。 */
#define RT_LWIP_UDP_PCB_NUM            4
/* TCP PCB 数量。 */
#define RT_LWIP_TCP_PCB_NUM            4
/* TCP segment 数量。 */
#define RT_LWIP_TCP_SEG_NUM            32
/* TCP 发送缓冲区大小。 */
#define RT_LWIP_TCP_SND_BUF            (8 * 1460)
/* TCP 接收窗口大小。 */
#define RT_LWIP_TCP_WND                (4 * 1460)
/* lwIP tcpip 线程优先级。 */
#define RT_LWIP_TCPTHREAD_PRIORITY     12
/* lwIP tcpip 线程栈大小。 */
#define RT_LWIP_TCPTHREAD_STACKSIZE    2048
/* lwIP tcpip 线程邮箱大小。 */
#define RT_LWIP_TCPTHREAD_MBOX_SIZE    32
/* RT-Thread ethernetif 收发线程优先级。 */
#define RT_LWIP_ETHTHREAD_PRIORITY     13
/* RT-Thread ethernetif 收发线程栈大小。 */
#define RT_LWIP_ETHTHREAD_STACKSIZE    2048
/* RT-Thread ethernetif 收发线程邮箱大小。 */
#define RT_LWIP_ETHTHREAD_MBOX_SIZE    32

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
