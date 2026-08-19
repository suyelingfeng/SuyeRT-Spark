/**
 * @file ui_navigation.h
 * @brief Owns screen transitions for the boot screen, main menu and feature pages.
 *
 * 职责：页面导航的对外接口。GUI 主循环调用 start/process 推进状态机；
 * 按键回调与 MSH 线程只通过 request_* 提交切换请求，避免在回调中删页面。
 */
#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include "lvgl.h"

/* 功能页编号：顺序与主菜单卡片、页面标题/配色表一一对应，新增页面需同步修改。 */
typedef enum
{
    UI_FEATURE_SENSORS = 0,
    UI_FEATURE_ENVIRONMENT,
    UI_FEATURE_ATTITUDE,
    UI_FEATURE_STORAGE,
    UI_FEATURE_NETWORK,
    UI_FEATURE_SYSTEM,
    UI_FEATURE_COUNT
} ui_feature_t;

/**
 * @brief 启动导航：显示开机画面，随后自动进入主菜单。
 * @param group 按键输入设备绑定的 LVGL 焦点组。
 */
void ui_navigation_start(lv_group_t *group);

/**
 * @brief 推进开机进度与延迟切换请求；由 GUI 主循环周期调用。
 */
void ui_navigation_process(void);

/**
 * @brief 立即打开指定功能页（仅 GUI 线程调用）。
 * @param feature 功能页编号，必须小于 UI_FEATURE_COUNT。
 */
void ui_navigation_open(ui_feature_t feature);

/**
 * @brief 线程安全的页面切换请求（供 MSH 使用），由 GUI 线程稍后执行。
 * @param feature 要打开的功能页编号。
 */
void ui_navigation_request_open(ui_feature_t feature);

/**
 * @brief 线程安全的回主菜单请求（供 MSH 使用），由 GUI 线程稍后执行。
 */
void ui_navigation_request_home(void);

/**
 * @brief 立即返回主菜单（仅 GUI 线程调用）。
 */
void ui_navigation_show_home(void);

/**
 * @brief 返回当前页面的英文名称，供串口诊断打印。
 * @retval const char* 静态字符串，如 "home"、"sensors"。
 */
const char *ui_navigation_current_name(void);

#endif /* UI_NAVIGATION_H */
