/**
 * @file ui_main_menu.c
 * @brief 240 x 240 six-function dashboard designed for four-key navigation.
 *
 * 职责：主菜单页——2 列 x 3 行共 6 张功能卡片，配合四向键导航。
 * 数据流：卡片 CLICKED 事件 -> ui_navigation_request_open() 提交延迟请求
 * -> GUI 主循环统一执行页面切换（不在事件回调中删除当前 screen）。
 */
#include "ui_main_menu.h"
#include "ui_navigation.h"
#include <rtthread.h>

/* Four-glyph subset generated from SimHei; it costs only a few KiB of Flash. */
LV_FONT_DECLARE(ui_font_chinese_14);

static lv_obj_t *hint_label;

typedef struct
{
    const char *title;
    const char *symbol;
    const char *hint;
    uint32_t accent;
    ui_feature_t feature;
} menu_item_t;

static const menu_item_t menu_items[] = {
    {"Sensors", LV_SYMBOL_EYE_OPEN, "All raw board sensors",       0x26C6DA, UI_FEATURE_SENSORS},
    {"Temp/RH", LV_SYMBOL_HOME,     "AHT21 Kalman filtered",       0x29B6F6, UI_FEATURE_ENVIRONMENT},
    {"Attitude", LV_SYMBOL_GPS,     "Quaternion + Kalman / zero",  0xEC407A, UI_FEATURE_ATTITUDE},
    {"Storage", LV_SYMBOL_SD_CARD,  "SDIO / Flash / FAL / DFS",   0xFFB74D, UI_FEATURE_STORAGE},
    {"Network", LV_SYMBOL_WIFI,     "RW007 WLAN on SPI2",         0x66BB6A, UI_FEATURE_NETWORK},
    {"System",  LV_SYMBOL_SETTINGS, "RT-Thread runtime status",   0xAB7BEF, UI_FEATURE_SYSTEM},
};

/* 卡片事件处理：焦点切换更新提示条，确认键提交延迟换页请求。 */
static void card_event(lv_event_t *event)
{
    const menu_item_t *item = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);

    if ((code == LV_EVENT_FOCUSED) && (hint_label != NULL))
    {
        lv_label_set_text(hint_label, item->hint);
    }
    else if ((code == LV_EVENT_CLICKED) && (hint_label != NULL))
    {
        rt_kprintf("[UI] Open: %s\n", item->title);
        /* Defer screen replacement until LVGL finishes dispatching this event. */
        ui_navigation_request_open(item->feature);
    }
    else if ((code == LV_EVENT_CANCEL) && (hint_label != NULL))
    {
        lv_label_set_text(hint_label, "Already at main menu");
    }
}

/* 创建单张功能卡片（按钮 + 图标 + 标题），并绑定 card_event。 */
static lv_obj_t *create_card(lv_obj_t *parent, const menu_item_t *item)
{
    lv_obj_t *card = lv_button_create(parent);
    lv_obj_t *symbol = lv_label_create(card);
    lv_obj_t *title = lv_label_create(card);

    lv_obj_set_size(card, 96, 46);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1B2942), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x243859), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x344866), 0);
    lv_obj_set_style_border_width(card, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(card, lv_color_hex(item->accent), LV_STATE_FOCUSED);
    /*
     * Do not scale buttons on LV_STATE_PRESSED.  Software transforms require
     * an off-screen layer in LVGL and can exhaust/lock the small MCU rendering
     * path before the key-release CLICKED event is generated.  A darker fill
     * gives clear feedback without allocating a transform layer.
     */
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111D30), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_add_event_cb(card, card_event, LV_EVENT_ALL, (void *)item);

    lv_label_set_text(symbol, item->symbol);
    lv_obj_set_style_text_color(symbol, lv_color_hex(item->accent), 0);
    lv_obj_set_style_text_font(symbol, &lv_font_montserrat_16, 0);
    lv_obj_align(symbol, LV_ALIGN_LEFT_MID, 8, 0);

    lv_label_set_text(title, item->title);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_RIGHT_MID, -7, 0);
    return card;
}

/**
 * @brief 创建主菜单 screen，并把 6 张卡片加入按键焦点组。
 * @param group 方向键导航使用的 LVGL 焦点组。
 * @retval lv_obj_t* 新建的 screen 对象指针（由调用方负责加载/删除）。
 * @note 适配 240x240 屏幕：卡片 96x46，2 列 3 行网格布局。
 */
lv_obj_t *ui_main_menu_create(lv_group_t *group)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_t *title = lv_label_create(header);
    lv_obj_t *badge = lv_label_create(header);
    lv_obj_t *grid = lv_obj_create(screen);
    static const int32_t columns[] = {96, 96, LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {46, 46, 46, LV_GRID_TEMPLATE_LAST};

    /*
     * The previous home screen may already have been auto-deleted.  Clear its
     * label pointer before adding cards because the first group object receives
     * LV_EVENT_FOCUSED immediately.
     */
    hint_label = NULL;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0D1729), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_set_size(header, 216, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_label_set_text(title, "RT-SPARK");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_label_set_text(badge, "夙夜凌风");
    lv_obj_set_style_text_color(badge, lv_color_hex(0xA7F3C1), 0);
    lv_obj_set_style_text_font(badge, &ui_font_chinese_14, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x174B35), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 8, 0);
    lv_obj_set_style_pad_ver(badge, 3, 0);
    lv_obj_set_style_pad_hor(badge, 7, 0);
    lv_obj_align(badge, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_size(grid, 204, 150);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 41);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, columns, rows);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    for (uint32_t i = 0; i < UI_FEATURE_COUNT; ++i)
    {
        lv_obj_t *card = create_card(grid, &menu_items[i]);
        lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, (int32_t)(i % 2U), 1,
                             LV_GRID_ALIGN_STRETCH, (int32_t)(i / 2U), 1);
        lv_group_add_obj(group, card);
    }

    hint_label = lv_label_create(screen);
    lv_label_set_text(hint_label, "UP/DN select  R enter  L back");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x8293AE), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_12, 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -7);

    /* Make the first card active immediately, so the user sees the focus style. */
    lv_group_focus_obj(lv_obj_get_child(grid, 0));
    return screen;
}
