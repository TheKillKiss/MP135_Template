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

#endif
