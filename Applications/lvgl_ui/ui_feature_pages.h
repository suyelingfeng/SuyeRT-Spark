/**
 * @file ui_feature_pages.h
 * @brief Four RT-Spark feature pages.
 *
 * 职责：功能页（传感器/温湿度/姿态/存储/网络/系统）的对外接口。
 * 上下游：由 ui_navigation 创建，数据来自 app_tasks 的板级服务快照。
 */
#ifndef UI_FEATURE_PAGES_H
#define UI_FEATURE_PAGES_H

#include "ui_navigation.h"

/**
 * @brief 创建指定功能页 screen，并把页面控件加入按键焦点组。
 * @param feature 功能页编号。
 * @param group 方向键导航使用的 LVGL 焦点组。
 * @retval lv_obj_t* 新建的 screen 对象指针。
 */
lv_obj_t *ui_feature_page_create(ui_feature_t feature, lv_group_t *group);

#endif /* UI_FEATURE_PAGES_H */
