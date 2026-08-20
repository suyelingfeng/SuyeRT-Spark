/** @file app_tasks.h  应用线程和线程间通信的唯一公共入口。 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "board_service.h"

/**
 * @brief 创建并启动 board / lvgl 两个应用线程，并初始化 FinSH。
 * @retval RT_EOK 全部启动成功；否则返回 RT-Thread 错误码。
 */
int app_tasks_start(void);

/**
 * @brief 请求 GUI 线程重绘当前屏幕（置标志，由 gui_thread 异步执行，
 *        调用方不得直接操作 LVGL 对象）。
 */
void app_tasks_request_ui_redraw(void);

/**
 * @brief 线程安全地拷贝一份板级服务快照。
 * @param snapshot 输出缓冲区，为 RT_NULL 时直接返回。
 */
void app_tasks_get_board_snapshot(board_service_snapshot_t *snapshot);

/** @brief 请求 board 线程立即刷新一次数据采集。 */
void app_tasks_request_board_refresh(void);

/** @brief 请求 board 线程复位 RW007 无线模块。 */
void app_tasks_request_rw007_reset(void);

/** @brief 请求 board 线程将姿态解算零点校准到当前姿态。 */
void app_tasks_request_attitude_zero(void);

/** @brief Request the board thread to select the next exclusive LED ring. */
void app_tasks_request_led_ring_next(void);

#endif
