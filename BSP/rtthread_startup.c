/**
 * @file rtthread_startup.c
 * @brief RT-Thread 内核启动序列：由 main() 调用，完成内核初始化并拉起调度器。
 *
 * 上下游：Core/Src/main.c 完成 HAL/时钟/GPIO/USART1 后调用 rtthread_startup()；
 * 本函数依次初始化堆、定时器、调度器，再经 rt_application_init() 创建
 * 应用线程（lvgl/board/tshell，见 Applications/runtime/app_tasks.c），
 * 最后启动调度器，正常情况下不再返回。
 */
#include "board.h"

extern int rt_application_init(void);
/* 链接脚本 STM32F407XX_FLASH.ld 定义的堆边界：从 .bss 末尾到 RAM 顶端预留 4 KB 处。 */
extern unsigned char __rt_heap_start__;
extern unsigned char __rt_heap_end__;

void rtthread_startup(void)
{
    /* 关全局中断，保证下面内核对象初始化不会被中断打断；调度器启动时由其恢复。 */
    rt_hw_interrupt_disable();

    /* 挂接系统堆：后续 rt_malloc/rt_thread_create 等动态内存都来自这段 RAM。 */
    rt_system_heap_init(&__rt_heap_start__, &__rt_heap_end__);
    /* 通过 rt_kprintf 打印内核版本横幅，同时验证 USART1 控制台通路可用。 */
    rt_show_version();
    /* 初始化软件定时器链表与节拍依赖，SysTick 节拍由 HAL 的 1 ms 中断提供。 */
    rt_system_timer_init();
    /* 初始化就绪队列等调度器数据结构，此时尚未开始调度。 */
    rt_system_scheduler_init();

    /* 创建应用线程；失败说明堆或配置有问题，直接断言定位，不带病继续启动。 */
    RT_ASSERT(rt_application_init() == RT_EOK);

    /* 创建软件定时器守护线程，RT_USING_TIMER_SOFT 下 rt_timer 的超时回调在此线程执行。 */
    rt_system_timer_thread_init();
    /* idle 线程必须最后就绪：调度器要求至少有一个可运行线程才能启动。 */
    rt_thread_idle_init();
    /* 僵尸线程回收队列初始化，用于 rt_thread_delete 后的资源延迟释放。 */
    rt_thread_defunct_init();
    /* 启动调度器：切换到最高优先级就绪线程，本函数正常不再返回。 */
    rt_system_scheduler_start();

    /* 兜底：调度器若能返回说明内核已异常，空转等待调试器介入。 */
    while (1)
    {
    }
}
