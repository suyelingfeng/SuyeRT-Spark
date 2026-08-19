/**
 * @file ui_boot_screen.c
 * @brief Non-blocking RT-Thread and "夙夜凌风" splash screen.
 *
 * 职责：创建并驱动开机画面（RT 徽标、品牌名、进度条）。
 * 数据流：ui_navigation_start() 创建本画面 -> ui_navigation_process()
 * 周期性调用 ui_boot_screen_set_progress() 更新进度 -> 计时到点后由
 * ui_navigation 切换到主菜单并删除本 screen。本文件只在 GUI 线程内被调用。
 */
#include "ui_boot_screen.h"

LV_FONT_DECLARE(ui_font_chinese_14);

/* 进度条填充块对象；只在开机画面存活期间有效，由 GUI 线程独占访问。 */
static lv_obj_t *progress_fill;

/**
 * @brief 更新开机进度条（0~100%）。
 * @param percent 进度百分比，超过 100 会被截断为 100。
 * @note 只允许 GUI 线程调用；进度条按 120 px 轨道宽度线性缩放填充块。
 */
void ui_boot_screen_set_progress(uint32_t percent)
{
    if (progress_fill != NULL)
    {
        if (percent > 100U) percent = 100U;
        lv_obj_set_width(progress_fill, (int32_t)(120U * percent / 100U));
    }
}

/**
 * @brief 创建开机画面 screen（不加载，由调用方负责 lv_screen_load()）。
 * @retval lv_obj_t* 新建的 screen 对象指针。
 * @note 画面元素全部为矢量样式和文字，避免在 Flash 中存放位图资源。
 */
lv_obj_t *ui_boot_screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *logo_box = lv_obj_create(screen);
    lv_obj_t *rt = lv_label_create(logo_box);
    lv_obj_t *name = lv_label_create(screen);
    lv_obj_t *brand = lv_label_create(screen);
    lv_obj_t *progress_track = lv_obj_create(screen);

    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080E1A), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* A compact vector-style RT badge avoids storing a large bitmap in Flash. */
    lv_obj_set_size(logo_box, 76, 76);
    lv_obj_align(logo_box, LV_ALIGN_CENTER, 0, -32);
    lv_obj_set_style_radius(logo_box, 18, 0);
    lv_obj_set_style_bg_color(logo_box, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(logo_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(logo_box, 0, 0);

    lv_label_set_text(rt, "RT");
    lv_obj_set_style_text_font(rt, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(rt, lv_color_white(), 0);
    lv_obj_center(rt);

    lv_label_set_text(name, "RT-THREAD");
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0xF4F7FC), 0);
    lv_obj_set_style_text_letter_space(name, 2, 0);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, 32);

    lv_label_set_text(brand, "夙夜凌风");
    lv_obj_set_style_text_font(brand, &ui_font_chinese_14, 0);
    lv_obj_set_style_text_color(brand, lv_color_hex(0xA7F3C1), 0);
    lv_obj_align(brand, LV_ALIGN_CENTER, 0, 62);

    /* 底部进度条：track 为深色轨道，fill 为红色填充块，宽度即进度。 */
    lv_obj_set_size(progress_track, 120, 5);
    lv_obj_align(progress_track, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_radius(progress_track, 3, 0);
    lv_obj_set_style_bg_color(progress_track, lv_color_hex(0x26364E), 0);
    lv_obj_set_style_bg_opa(progress_track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(progress_track, 0, 0);
    lv_obj_set_style_pad_all(progress_track, 0, 0);
    lv_obj_remove_flag(progress_track, LV_OBJ_FLAG_SCROLLABLE);

    progress_fill = lv_obj_create(progress_track);
    lv_obj_set_size(progress_fill, 0, 5);
    lv_obj_align(progress_fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(progress_fill, 3, 0);
    lv_obj_set_style_bg_color(progress_fill, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(progress_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(progress_fill, 0, 0);
    lv_obj_set_style_pad_all(progress_fill, 0, 0);
    return screen;
}
