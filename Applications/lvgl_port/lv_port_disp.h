/** @file lv_port_disp.h  LVGL 显示适配层：LVGL 渲染结果经 FSMC 刷到板载 240x240 ST7789 LCD。 */
#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include "lvgl.h"

/**
 * @brief 向 LVGL 注册 240 x 240 RGB565 显示屏（局部刷新模式，FSMC 同步刷图）。
 * @retval 非 NULL  成功，返回 LVGL 显示句柄。
 * @retval NULL     失败（LVGL 内存不足）。
 * @note 必须在 lv_init() 之后调用；所有 lv_* API 只允许在 gui_thread 中使用。
 */
lv_display_t *lv_port_disp_init(void);

#endif /* LV_PORT_DISP_H */
