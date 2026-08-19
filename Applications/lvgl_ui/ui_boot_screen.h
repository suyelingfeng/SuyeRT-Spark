/**
 * @file ui_boot_screen.h
 * @brief Animated RT-Thread and suye boot screen.
 *
 * 职责：开机画面（splash）的对外接口。
 * 上下游：由 ui_navigation 创建/驱动，只在 GUI 线程内使用。
 */
#ifndef UI_BOOT_SCREEN_H
#define UI_BOOT_SCREEN_H

#include "lvgl.h"

/**
 * @brief 创建开机画面 screen（不加载，由调用方负责 lv_screen_load()）。
 * @retval lv_obj_t* 新建的 screen 对象指针。
 */
lv_obj_t *ui_boot_screen_create(void);

/**
 * @brief 更新开机进度条。
 * @param percent 进度百分比（0~100，超出会被截断）。
 * @note 只允许 GUI 线程调用（LVGL 非线程安全）。
 */
void ui_boot_screen_set_progress(uint32_t percent);

#endif /* UI_BOOT_SCREEN_H */
