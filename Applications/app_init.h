/** @file app_init.h  RT-Thread 应用启动钩子声明。 */
#ifndef APP_INIT_H
#define APP_INIT_H

/**
 * @brief RT-Thread 应用初始化钩子，负责创建并启动全部应用线程。
 * @retval RT_EOK 成功。
 */
int rt_application_init(void);

#endif /* APP_INIT_H */
