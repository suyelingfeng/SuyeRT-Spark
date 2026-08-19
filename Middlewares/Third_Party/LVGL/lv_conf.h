/**
 * @file lv_conf.h
 * @brief LVGL 9.5 configuration for the STM32F407 RT-Spark board.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* The LCD uses RGB565.  Keep the renderer in the same format to avoid conversion. */
#define LV_COLOR_DEPTH 16

/* Use LVGL's compact internal allocator; 32 KiB is sufficient for this menu. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (32U * 1024U)

/* LVGL is called from one RT-Thread GUI thread, so no LVGL OS wrapper is needed. */
#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 20
#define LV_DPI_DEF 130

/* Keep the firmware small: only the fonts used by this page are compiled. */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LOG 0
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
