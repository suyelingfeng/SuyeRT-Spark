/**
 * @file st7789_fsmc.h
 * @brief Minimal ST7789V3 driver for the RT-Spark 240 x 240 LCD.
 *
 * 数据流：LVGL flush 回调（Applications/lvgl_port/lv_port_disp.c）
 * -> st7789_blit() -> FSMC 8080 并口（8 位数据 + A18 作 D/C）；
 * MSH 诊断命令 -> st7789_fill_color()/st7789_set_backlight()。
 * 初始化由 gui_thread 在 LVGL 初始化前调用一次（见 Applications/runtime/app_tasks.c）。
 */
#ifndef ST7789_FSMC_H
#define ST7789_FSMC_H

#include <stddef.h>
#include <stdint.h>

/* 面板有效分辨率，同时也是 0x2A/0x2B 窗口坐标的合法范围。 */
#define ST7789_WIDTH  240U
#define ST7789_HEIGHT 240U

/**
 * @brief 初始化复位/背光 GPIO、FSMC 总线和 ST7789 控制器。
 * @retval 0 成功；-1 互斥量创建或 FSMC 初始化失败。
 */
int st7789_init(void);

/**
 * @brief 设置后续像素流写入的 GRAM 矩形窗口（坐标含端点）。
 * @param x1 起始列，x2 结束列；y1 起始行，y2 结束行。
 * @note 本函数本身不加锁；与 st7789_write_pixels() 配对刷新时应使用
 *       st7789_blit()，由它保证"窗口命令 + 像素流"的原子性。
 */
void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief 向当前窗口顺序写入 RGB565 像素。
 * @param pixels RGB565 像素数组；count 像素个数（须与窗口面积一致）。
 */
void st7789_write_pixels(const uint16_t *pixels, size_t count);

/**
 * @brief 加总线互斥锁地刷新一个矩形区域，避免与 MSH 诊断命令交叉访问总线。
 * @param x1/y1 左上角坐标；x2/y2 右下角坐标（均含端点）。
 * @param pixels 待写入的 RGB565 像素；count 像素个数。
 */
void st7789_blit(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                 const uint16_t *pixels, size_t count);

/** @brief 用单一 RGB565 颜色填满屏幕，供 lcd_test 命令诊断。 */
void st7789_fill_color(uint16_t color);

/**
 * @brief 设置背光开关。RT-Spark 原理图中背光控制为 PF9，高电平点亮。
 * @param on 非 0 点亮；0 熄灭。
 */
void st7789_set_backlight(int on);

/**
 * @brief 读取背光状态。
 * @retval 非 0 背光点亮；0 熄灭。
 */
int st7789_get_backlight(void);

/**
 * @brief 查询 LCD 是否已完成初始化。
 * @retval 非 0 已就绪；0 未初始化或初始化失败。
 */
int st7789_is_ready(void);

/**
 * @brief 读取控制器 ID（0x04 命令返回值，正常为 0x81B3）。
 * @retval 控制器 ID。
 */
uint16_t st7789_get_id(void);

#endif /* ST7789_FSMC_H */
