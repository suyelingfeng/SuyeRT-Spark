/**
 * @file rtconfig.h
 * @brief RT-Thread 内核裁剪配置文件：以功能分组宏开关决定编译进固件的内核组件与资源上限。
 *
 * 本文件只有宏定义，由 rtthread.h 在编译期包含；任何改动都需要全量重编译才能生效。
 * 上游：rtthread_startup.c（启动时按这里的配置初始化堆、调度器与线程）；
 * 下游：所有调用 RT-Thread API 的 BSP 驱动、服务与 Applications 线程。
 */
#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/* ===== 内核基础参数 =====
 * RT_ALIGN_SIZE=8 满足 Cortex-M 的 AAPCS 栈对齐要求，改了会破坏中断现场的浮点/64 位数据访问。 */
#define RT_NAME_MAX                     12
#define RT_CPUS_NR                      1
#define RT_ALIGN_SIZE                   8

/* ===== 调度与线程 =====
 * RT_TICK_PER_SECOND=1000 即 1ms 一个系统节拍（由 SysTick 驱动），
 * 是 rt_thread_mdelay 与所有超时参数的时间基准，改大会更省电但降低定时精度；
 * RT_USING_OVERFLOW_CHECK 在任务切换时检查线程栈是否越界。 */
#define RT_THREAD_PRIORITY_32
#define RT_THREAD_PRIORITY_MAX          32
#define RT_TICK_PER_SECOND              1000
#define RT_USING_OVERFLOW_CHECK
#define IDLE_THREAD_STACK_SIZE          512

/* ===== 调试支持 =====
 * 断言失败会打印文件/行号并挂死系统，Color 仅影响 rt_kprintf 日志着色。 */
#define RT_USING_DEBUG
#define RT_DEBUGING_ASSERT
#define RT_DEBUGING_COLOR

/* ===== 线程间通信（IPC） =====
 * 本工程的 app_tasks / 服务线程依赖互斥锁与消息队列做跨线程请求传递，裁剪时不要关闭。 */
#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE

/* ===== 内存管理 =====
 * 堆采用 small memory 算法，堆区间由链接脚本符号 __rt_heap_start__/__rt_heap_end__
 * 圈定（见 rtthread_startup.c 的 rt_system_heap_init），不在本文件配置大小。 */
#define RT_USING_MEMPOOL
#define RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM_AS_HEAP
#define RT_USING_HEAP

/* ===== 控制台输出 =====
 * RT_CONSOLEBUF_SIZE 是 rt_kprintf 的单次输出缓冲，日志行过长会被截断。 */
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE              128

/* ===== FinSH/MSH 命令行 =====
 * tshell 线程优先级 20，低于 gui_thread(18)/board_thread(19)，shell 卡死不会拖垮业务线程；
 * 栈 4096 是为带历史与符号表的 msh 预留，减小后运行带格式化输出的命令容易栈溢出。 */
#define RT_USING_FINSH
#define FINSH_USING_MSH
#define FINSH_THREAD_NAME               "tshell"
#define FINSH_THREAD_PRIORITY           20
#define FINSH_THREAD_STACK_SIZE         4096
#define FINSH_USING_HISTORY
#define FINSH_HISTORY_LINES             5
#define FINSH_USING_SYMTAB
#define FINSH_USING_DESCRIPTION
#define FINSH_CMD_SIZE                  128
#define FINSH_ARG_MAX                   10
#define MSH_USING_BUILT_IN_COMMANDS

/* ===== 版本与异常回溯 =====
 * RT_BACKTRACE_LEVEL_MAX_NR 限制 hardfault 时打印的最大调用栈深度。 */
#define RT_VER_NUM                      0x50202
#define RT_BACKTRACE_LEVEL_MAX_NR       32

/* ===== 硬件加速原语 =====
 * 使用 Cortex-M 的 LDREX/STREX 实现原子操作、CLZ 指令加速位图查找，
 * 比纯软件实现的关中断临界区更短，要求内核移植层与编译器支持。 */
#define RT_USING_HW_ATOMIC
#define RT_USING_CPU_FFS

/* ===== 架构与芯片/板级标识 =====
 * 决定 libcpu 选择 ARM Cortex-M4 移植层（PendSV 切换、SysTick 节拍）。 */
#define ARCH_ARM
#define ARCH_ARM_CORTEX_M
#define ARCH_ARM_CORTEX_M4
#define SOC_STM32F407ZG
#define BOARD_STM32F407_SPARK

#endif
