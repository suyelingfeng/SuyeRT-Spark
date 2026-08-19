/** @file lv_port_indev.h  LVGL 输入适配层：板上四个方向键 -> LVGL 键盘输入与焦点组。 */
#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#include "lvgl.h"

/** lv_port_indev_get_key_mask() 返回的物理按键位定义；低电平按下，已转成"按下=置位"。 */
#define LV_PORT_KEY_LEFT  (1U << 0)
#define LV_PORT_KEY_DOWN  (1U << 1)
#define LV_PORT_KEY_RIGHT (1U << 2)
#define LV_PORT_KEY_UP    (1U << 3)

/**
 * @brief 初始化四个方向键并注册 LVGL 键盘输入设备，返回新建的 LVGL 焦点组。
 * @retval 非 NULL  成功，返回焦点组句柄。
 * @retval NULL     失败（LVGL 内存不足）。
 */
lv_group_t *lv_port_indev_init(void);

/**
 * @brief 读取四个按键的原始低电平状态并合成位掩码（未消抖）。
 * @retval 位掩码，按下位为 LV_PORT_KEY_xxx；全 0 表示无键按下。
 * @note 主要供 shell 诊断命令使用；正常 UI 流程走 LVGL 输入设备回调。
 */
uint32_t lv_port_indev_get_key_mask(void);

/**
 * @brief 切屏前丢弃触发切屏的那次按键事件，并等待所有键物理松开。
 *
 * 防止这次按键的释放事件被派发到新建屏幕的控件上，造成误触发。
 */
void lv_port_indev_begin_screen_change(void);

#endif /* LV_PORT_INDEV_H */
