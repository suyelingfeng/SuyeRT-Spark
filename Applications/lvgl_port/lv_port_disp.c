/**
 * @file lv_port_disp.c
 * @brief 把 LVGL 的局部渲染结果经 FSMC 总线刷到 ST7789 LCD。
 *
 * 数据流：gui_thread 调 lv_timer_handler() -> LVGL 局部渲染到 draw_buffer
 * -> display_flush() -> st7789_blit()（FSMC 8 位总线，阻塞写）-> LCD 显存。
 * LVGL 不是线程安全库，本文件所有代码都只在 gui_thread 上下文运行。
 */
#include "lv_port_disp.h"
#include "st7789_fsmc.h"

#include <rtthread.h>

/* 24 行局部缓冲只占 11.25 KiB；整帧 240x240 RGB565 要 112.5 KiB，片内 SRAM 放不下。 */
static uint16_t draw_buffer[ST7789_WIDTH * 24U] __attribute__((aligned(4)));

/* LVGL 的动画与消抖都依赖 tick，直接复用 RT-Thread 系统节拍，保证时间基准一致。 */
static uint32_t lvgl_tick_ms(void)
{
    return (uint32_t)rt_tick_get_millisecond();
}

/* LVGL 每渲染完一个脏矩形就回调这里；pixel_map 指向 draw_buffer，可直接刷屏。 */
static void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map)
{
    const uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);

    /* LVGL 以 uint8_t* 传像素指针，RGB565 格式下实际按 16 位像素逐点解释。 */
    st7789_blit((uint16_t)area->x1, (uint16_t)area->y1,
                (uint16_t)area->x2, (uint16_t)area->y2,
                (const uint16_t *)pixel_map, width * height);

    /* st7789_blit 是同步传输（FSMC 阻塞写，无 DMA），返回时数据已写入 LCD，
       因此可以立刻通知 LVGL 复用 draw_buffer。 */
    lv_display_flush_ready(display);
}

/**
 * @brief 创建并向 LVGL 注册 240 x 240 RGB565 显示设备。
 *
 * tick 回调必须先装好，否则后续创建的 LVGL 对象拿不到正确的时间基准。
 * 采用单缓冲 + LV_DISPLAY_RENDER_MODE_PARTIAL：LVGL 分块渲染进
 * draw_buffer，flush 同步完成后立即复用，用串行刷新换取内存占用。
 *
 * @retval 非 NULL  成功，返回 LVGL 显示句柄。
 * @retval NULL     失败（LVGL 内存不足）。
 */
lv_display_t *lv_port_disp_init(void)
{
    lv_display_t *display;

    lv_tick_set_cb(lvgl_tick_ms);
    display = lv_display_create(ST7789_WIDTH, ST7789_HEIGHT);
    if (display == NULL)
    {
        return NULL;
    }

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return display;
}
