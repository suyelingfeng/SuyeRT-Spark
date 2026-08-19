/**
 * @file ui_main_menu.h
 * @brief Main 2 x 2 application menu.
 *
 * 职责：主菜单页对外接口（2 列 x 3 行卡片式仪表盘）。
 * 上下游：由 ui_navigation 创建，卡片事件通过 request_open 延迟换页。
 */
#ifndef UI_MAIN_MENU_H
#define UI_MAIN_MENU_H

#include "lvgl.h"

/**
 * @brief 创建主菜单 screen，并把卡片加入按键焦点组。
 * @param group 方向键导航使用的 LVGL 焦点组。
 * @retval lv_obj_t* 新建的 screen 对象指针。
 */
lv_obj_t *ui_main_menu_create(lv_group_t *group);

#endif /* UI_MAIN_MENU_H */
