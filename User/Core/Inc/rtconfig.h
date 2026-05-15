#ifndef RTCONFIG_H__
#define RTCONFIG_H__

#define RT_NAME_MAX                    8
#define RT_ALIGN_SIZE                  8
#define RT_THREAD_PRIORITY_MAX         32
#define RT_TICK_PER_SECOND             1000
#define RT_CPUS_NR                     1

#define IDLE_THREAD_STACK_SIZE         512
#define RT_MAIN_THREAD_STACK_SIZE      1024
#define RT_MAIN_THREAD_PRIORITY        10
#define RT_BACKTRACE_LEVEL_MAX_NR      16
#define RT_KLIBC_USING_LIBC_VSNPRINTF
#define RT_USING_NANO
#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_HEAP
#define RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM_AS_HEAP
#define RT_HEAP_SIZE                   (32 * 1024)

#define RT_USING_DEVICE
#define RT_USING_SERIAL
#define RT_USING_SERIAL_V1
#define RT_SERIAL_RB_BUFSZ             256

#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE             256
#define RT_CONSOLE_DEVICE_NAME         "uart4"

#define RT_USING_MSH
#define RT_USING_FINSH
#define FINSH_USING_MSH
#define FINSH_USING_SYMTAB
#define FINSH_USING_DESCRIPTION
#define FINSH_THREAD_NAME              "tshell"
#define FINSH_THREAD_PRIORITY          20
#define FINSH_THREAD_STACK_SIZE        2048
#define FINSH_CMD_SIZE                 128
#define FINSH_ARG_MAX                  10
#define MSH_USING_BUILT_IN_COMMANDS

#endif
