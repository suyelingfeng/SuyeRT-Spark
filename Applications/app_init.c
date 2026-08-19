/** @file app_init.c  RT-Thread 应用启动钩子。 */
#include "app_init.h"
#include "app_tasks.h"

/**
 * @brief RT-Thread 应用初始化钩子，由 BSP/rtthread_startup.c 在调度器启动前调用。
 * @retval RT_EOK 线程启动成功；出错时返回值会触发调用处的 RT_ASSERT。
 */
int rt_application_init(void)
{
    /* 线程定义、优先级、栈和启动顺序统一在 app_tasks.c。 */
    return app_tasks_start();
}
