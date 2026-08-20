/**
 * @file ui_navigation.c
 * @brief Centralized LVGL screen ownership and transitions.
 *
 * 职责：统一管理开机画面、主菜单、功能页三类 screen 的创建、切换与销毁。
 * 数据流：按键事件 / MSH 命令 -> ui_navigation_request_*() 仅记录请求 ->
 * GUI 主循环每约 5 ms 调用 ui_navigation_process() 集中执行真正的页面切换。
 * 这样保证 LVGL 对象的创建/删除只发生在 GUI 线程主循环里，
 * 而不是发生在控件事件回调或串口 shell 线程中。
 */
#include "ui_navigation.h"
#include "ui_boot_screen.h"
#include "ui_feature_pages.h"
#include "ui_main_menu.h"
#include "lv_port_indev.h"
#include <rtthread.h>

static lv_group_t *navigation_group;
static const char *current_page_name = "not-started";
static uint32_t boot_started_at;
static bool boot_active;
static volatile int pending_page_request = -1;

/*
 * 页面请求编码：-1=无请求，0=返回主页，feature+1=打开功能页。
 * 按键事件和 shell 只提交请求，真正的 screen 创建/删除统一在 GUI 主循环
 * 调用 ui_navigation_process() 时执行，从而避免在控件回调中删除当前页面。
 */

static void load_screen(lv_obj_t *screen, lv_screen_load_anim_t animation)
{
    lv_obj_t *old_screen = lv_screen_active();
    LV_UNUSED(animation);

    /*
     * Do not call lv_screen_load_anim(), even with animation NONE.  LVGL keeps
     * a prev_scr while its animation state machine completes and deliberately
     * stops reading every input device during that period.  Direct loading and
     * deleting is safe here because requests are processed outside callbacks.
     */
    rt_kprintf("[UI-NAV] input release guard\n");
    lv_port_indev_begin_screen_change();
    rt_kprintf("[UI-NAV] load new screen\n");
    lv_screen_load(screen);
    if ((old_screen != NULL) && (old_screen != screen))
    {
        rt_kprintf("[UI-NAV] delete old screen\n");
        lv_obj_delete(old_screen);
    }
    rt_kprintf("[UI-NAV] switch complete\n");
}

/**
 * @brief 立即切换到主菜单（开机结束或收到回主页请求时由 GUI 线程调用）。
 * @note 会清空焦点组并重建主菜单，开机后首次进入时使用淡入动画。
 */
void ui_navigation_show_home(void)
{
    lv_obj_t *screen;
    bool from_boot = boot_active;
    if (navigation_group == NULL) return;

    boot_active = false;
    rt_kprintf("[UI-NAV] clear home focus group\n");
    lv_group_remove_all_objs(navigation_group);
    rt_kprintf("[UI-NAV] create home\n");
    screen = ui_main_menu_create(navigation_group);
    current_page_name = "home";
    load_screen(screen, from_boot ? LV_SCREEN_LOAD_ANIM_FADE_IN : LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
    rt_kprintf("[UI] Home page\n");
}

/**
 * @brief 立即打开指定功能页（由 GUI 线程在处理请求时调用）。
 * @param feature 功能页编号，必须小于 UI_FEATURE_COUNT。
 */
void ui_navigation_open(ui_feature_t feature)
{
    lv_obj_t *screen;
    if ((navigation_group == NULL) || (feature >= UI_FEATURE_COUNT)) return;

    rt_kprintf("[UI-NAV] clear feature focus group\n");
    lv_group_remove_all_objs(navigation_group);
    rt_kprintf("[UI-NAV] create feature %d\n", (int)feature);
    screen = ui_feature_page_create(feature, navigation_group);
    switch (feature)
    {
    case UI_FEATURE_SENSORS: current_page_name = "sensors"; break;
    case UI_FEATURE_ENVIRONMENT: current_page_name = "environment"; break;
    case UI_FEATURE_ATTITUDE: current_page_name = "attitude"; break;
    case UI_FEATURE_STORAGE: current_page_name = "storage"; break;
    case UI_FEATURE_NETWORK: current_page_name = "network"; break;
    case UI_FEATURE_SYSTEM:  current_page_name = "system"; break;
    case UI_FEATURE_LED_RINGS: current_page_name = "led-rings"; break;
    case UI_FEATURE_GPIO_PINS: current_page_name = "gpio-pins"; break;
    default:                 current_page_name = "unknown"; break;
    }
    load_screen(screen, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
}

/**
 * @brief 提交打开功能页的延迟请求（线程安全，可由 MSH 线程调用）。
 * @param feature 功能页编号；真正的切换在 ui_navigation_process() 中执行。
 */
void ui_navigation_request_open(ui_feature_t feature)
{
    if (feature < UI_FEATURE_COUNT)
    {
        pending_page_request = (int)feature + 1;
    }
}

/**
 * @brief 提交返回主菜单的延迟请求（线程安全，可由 MSH 线程调用）。
 */
void ui_navigation_request_home(void)
{
    pending_page_request = 0;
}

/**
 * @brief 启动导航：显示开机画面并记录起始节拍，之后由 ui_navigation_process() 推进。
 * @param group 按键输入设备绑定的 LVGL 焦点组，后续页面复用。
 */
void ui_navigation_start(lv_group_t *group)
{
    lv_obj_t *splash;
    navigation_group = group;
    current_page_name = "boot-animation";
    boot_started_at = lv_tick_get();
    boot_active = true;
    lv_group_remove_all_objs(navigation_group);
    splash = ui_boot_screen_create();
    lv_screen_load(splash);
}

/**
 * @brief 推进开机计时并执行延迟的页面切换请求；由 GUI 主循环每约 5 ms 调用一次。
 * @note 开机画面总时长 1600 ms，进度条按已耗时比例更新。
 */
void ui_navigation_process(void)
{
    /* 此函数由 gui_thread_entry() 每约 5 ms 调用一次。 */
    if (boot_active)
    {
        uint32_t elapsed = lv_tick_diff(lv_tick_get(), boot_started_at);
        ui_boot_screen_set_progress((elapsed * 100U) / 1600U);
        if (elapsed >= 1600U)
        {
            ui_navigation_show_home();
        }
    }
    else if (pending_page_request >= 0)
    {
        int request = pending_page_request;
        pending_page_request = -1;
        if (request == 0)
        {
            ui_navigation_show_home();
        }
        else
        {
            ui_navigation_open((ui_feature_t)(request - 1));
        }
    }
}

/**
 * @brief 返回当前页面的英文名称（供串口诊断打印）。
 * @retval const char* 静态字符串，如 "home"、"sensors"、"boot-animation"。
 */
const char *ui_navigation_current_name(void)
{
    return current_page_name;
}
