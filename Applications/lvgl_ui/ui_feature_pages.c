/**
 * @file ui_feature_pages.c
 * @brief First-stage pages based on the official RT-Spark BSP device layout.
 *
 * The UI is deliberately separated from device drivers.  Sensor, DFS/FAL and
 * WLAN providers can replace the status functions later without redesigning
 * the screens or key navigation.
 */
#include "ui_feature_pages.h"
#include "app_tasks.h"
#include <rtthread.h>
#include <stdlib.h>

static ui_feature_t active_feature;
static lv_obj_t *status_label;
static lv_timer_t *refresh_timer;

static const char *feature_title(ui_feature_t feature)
{
    static const char *titles[] = {
        "SENSORS", "TEMP / RH", "ATTITUDE", "STORAGE", "NETWORK", "SYSTEM"
    };
    return titles[(uint32_t)feature];
}

/* 功能页主题色表：与主菜单卡片配色保持一致。 */
static uint32_t feature_accent(ui_feature_t feature)
{
    static const uint32_t colors[] = {
        0x26C6DA, 0x29B6F6, 0xEC407A, 0xFFB74D, 0x66BB6A, 0xAB7BEF
    };
    return colors[(uint32_t)feature];
}

/* 读取板级服务原子快照，按当前功能页格式化到 status_label。 */
static void refresh_status(void)
{
    board_service_snapshot_t board;
    if (status_label == NULL)
    {
        return;
    }
    app_tasks_get_board_snapshot(&board);

    switch (active_feature)
    {
    case UI_FEATURE_SENSORS:
        if (board.sequence == 0U)
        {
            lv_label_set_text(status_label, "Starting sensor service...\n\nPlease wait for first sample.");
        }
        else
        {
            int temp_fraction = board.temperature_x10 % 10;
            if (temp_fraction < 0) temp_fraction = -temp_fraction;
            lv_label_set_text_fmt(status_label,
                                  "AHT21   %s  %d.%d C  %u.%u %%\n"
                                  "AP3216  %s  %lu.%lu lx  PS:%u\n"
                                  "ICM206  %s\n"
                                  "A:%+d  %+d  %+d mg\n"
                                  "G(c):%+d  %+d  %+d x0.1dps",
                                  board.aht21_ok ? "OK" : "--",
                                  board.temperature_x10 / 10, temp_fraction,
                                  board.humidity_x10 / 10U, board.humidity_x10 % 10U,
                                  board.ap3216c_ok ? "OK" : "--",
                                  (unsigned long)(board.ambient_light_x10 / 10U),
                                  (unsigned long)(board.ambient_light_x10 % 10U), board.proximity,
                                  board.icm20608_ok ? "OK" : "--",
                                  board.accel_mg[0], board.accel_mg[1], board.accel_mg[2],
                                  board.gyro_dps_x10[0], board.gyro_dps_x10[1], board.gyro_dps_x10[2]);
        }
        break;

    case UI_FEATURE_ENVIRONMENT:
        if (!board.environment_filter_ready)
        {
            lv_label_set_text(status_label,
                              "AHT21 + scalar Kalman\n\n"
                              "Waiting for first sample...");
        }
        else
        {
            int raw_t_frac = board.temperature_x10 % 10;
            int filtered_t_frac = board.temperature_kalman_x10 % 10;
            if (raw_t_frac < 0) raw_t_frac = -raw_t_frac;
            if (filtered_t_frac < 0) filtered_t_frac = -filtered_t_frac;
            lv_label_set_text_fmt(status_label,
                                  "AHT21       %s\n"
                                  "Temp raw    %d.%d C\n"
                                  "Temp Kalman %d.%d C\n"
                                  "RH raw      %u.%u %%\n"
                                  "RH Kalman   %u.%u %%\n"
                                  "Q/R T:.02/.30 H:.05/.80",
                                  board.aht21_ok ? "OK" : "ERROR",
                                  board.temperature_x10 / 10, raw_t_frac,
                                  board.temperature_kalman_x10 / 10, filtered_t_frac,
                                  board.humidity_x10 / 10U, board.humidity_x10 % 10U,
                                  board.humidity_kalman_x10 / 10U,
                                  board.humidity_kalman_x10 % 10U);
        }
        break;

    case UI_FEATURE_ATTITUDE:
        if (!board.icm20608_ok)
        {
            lv_label_set_text(status_label, "ICM20608 not detected");
        }
        else if (!board.attitude_ready)
        {
            lv_label_set_text_fmt(status_label,
                                  "Gyro zero calibration\n\n"
                                  "Keep board completely still\n"
                                  "Samples  %u / %u\n\n"
                                  "Movement restarts counting",
                                  board.attitude_calibration_samples,
                                  board.attitude_calibration_target);
        }
        else
        {
            lv_label_set_text_fmt(status_label,
                                  "Roll   %+d.%d deg\n"
                                  "Pitch  %+d.%d deg\n"
                                  "Yaw*   %+d.%d deg\n"
                                  "Q x10000  %+d  %+d\n"
                                  "          %+d  %+d\n"
                                  "ZUPT %s  IMU %d.%d C",
                                  board.roll_x10 / 10, abs(board.roll_x10 % 10),
                                  board.pitch_x10 / 10, abs(board.pitch_x10 % 10),
                                  board.yaw_x10 / 10, abs(board.yaw_x10 % 10),
                                  board.quaternion_x10000[0],
                                  board.quaternion_x10000[1],
                                  board.quaternion_x10000[2],
                                  board.quaternion_x10000[3],
                                  board.attitude_stationary ? "STILL" : "MOVE",
                                  board.imu_temperature_x10 / 10,
                                  abs(board.imu_temperature_x10 % 10));
        }
        break;

    case UI_FEATURE_STORAGE:
        lv_label_set_text_fmt(status_label,
                              "SPI Flash  %s\n"
                              "JEDEC      %02X %02X %02X\n"
                              "Capacity   %lu KiB\n"
                              "SD card    %s\n\n"
                              "Reserved: /fal  /sdcard",
                              !board.flash_ok ? "NOT FOUND" :
                              (board.flash_jedec[2] == 0x17U ? "W25Q64 OK" :
                              (board.flash_jedec[2] == 0x18U ? "W25Q128 OK" : "WINBOND OK")),
                              board.flash_jedec[0], board.flash_jedec[1], board.flash_jedec[2],
                              (unsigned long)board.flash_size_kib,
                              board.sd_inserted ? "INSERTED" : "NOT INSERTED");
        break;

    case UI_FEATURE_NETWORK:
        lv_label_set_text_fmt(status_label,
                              "Module     RW007\n"
                              "Hardware   %s\n"
                              "RST        %s\n"
                              "INT/BUSY   %s\n"
                              "SPI2       5.25 MHz\n"
                              "Resets     %lu",
                              board.rw007_ready ? "READY" : "BUSY/OFFLINE",
                              board.rw007_reset_released ? "RELEASED" : "LOW",
                              board.rw007_int_high ? "HIGH" : "LOW",
                              (unsigned long)board.rw007_reset_count);
        break;

    case UI_FEATURE_SYSTEM:
    default:
    {
        rt_size_t total = 0U;
        rt_size_t used = 0U;
        rt_size_t maximum = 0U;
        uint32_t uptime = (uint32_t)(rt_tick_get() / RT_TICK_PER_SECOND);
        int thread_count = rt_object_get_length(RT_Object_Class_Thread);
        rt_memory_info(&total, &used, &maximum);
        lv_label_set_text_fmt(status_label,
                              "RT-Thread   5.2.2\n"
                              "CPU         168 MHz\n"
                              "Uptime      %lu s\n"
                              "Threads     %d\n"
                              "Heap        %lu / %lu B",
                              (unsigned long)uptime, thread_count,
                              (unsigned long)used, (unsigned long)total);
        break;
    }
    }
}

/* 500 ms 周期刷新定时器回调。 */
static void refresh_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    refresh_status();
}

/* screen 删除时清空静态指针并销毁刷新定时器，防止定时器访问已释放对象。 */
static void page_screen_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_DELETE)
    {
        status_label = NULL;
        if (refresh_timer != NULL)
        {
            lv_timer_delete(refresh_timer);
            refresh_timer = NULL;
        }
    }
}

/* 底部按钮事件：确认键按页面类型提交复位/校零/刷新请求，取消键延迟回主页。 */
static void page_button_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_CLICKED)
    {
        if (active_feature == UI_FEATURE_NETWORK)
            app_tasks_request_rw007_reset();
        else if (active_feature == UI_FEATURE_ATTITUDE)
            app_tasks_request_attitude_zero();
        else
            app_tasks_request_board_refresh();
        refresh_status();
        rt_kprintf("[UI] Refreshed %s page\n", feature_title(active_feature));
    }
    else if (code == LV_EVENT_CANCEL)
    {
        /* Never delete/replace the active screen from inside its own event callback. */
        ui_navigation_request_home();
    }
}

/**
 * @brief 创建指定功能页 screen，并把刷新按钮加入按键焦点组。
 * @param feature 功能页编号，决定标题、配色和状态区内容。
 * @param group 方向键导航使用的 LVGL 焦点组。
 * @retval lv_obj_t* 新建的 screen 对象指针（由调用方负责加载/删除）。
 * @note 创建后立即刷新一次状态，并注册 500 ms 周期刷新定时器。
 */
lv_obj_t *ui_feature_page_create(ui_feature_t feature, lv_group_t *group)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_t *back_hint = lv_label_create(header);
    lv_obj_t *title = lv_label_create(header);
    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_t *refresh_button = lv_button_create(screen);
    lv_obj_t *refresh_text = lv_label_create(refresh_button);
    uint32_t accent = feature_accent(feature);

    active_feature = feature;
    lv_obj_add_event_cb(screen, page_screen_event, LV_EVENT_DELETE, NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0D1729), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_set_size(header, 216, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_label_set_text(back_hint, LV_SYMBOL_LEFT " LEFT");
    lv_obj_set_style_text_font(back_hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(back_hint, lv_color_hex(0x8293AE), 0);
    lv_obj_align(back_hint, LV_ALIGN_LEFT_MID, 0, 0);

    lv_label_set_text(title, feature_title(feature));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent), 0);
    lv_obj_align(title, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_size(panel, 216, 136);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x17243A), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x30435F), 0);
    lv_obj_set_style_pad_all(panel, 10, 0);

    status_label = lv_label_create(panel);
    lv_obj_set_width(status_label, 194);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xDCE5F2), 0);
    lv_obj_set_style_text_line_space(status_label, 3, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_set_size(refresh_button, 92, 34);
    lv_obj_align(refresh_button, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_radius(refresh_button, 10, 0);
    lv_obj_set_style_bg_color(refresh_button, lv_color_hex(0x243859), 0);
    lv_obj_set_style_border_width(refresh_button, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(refresh_button, lv_color_hex(accent), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(refresh_button, page_button_event, LV_EVENT_ALL, NULL);

    if (feature == UI_FEATURE_NETWORK)
        lv_label_set_text(refresh_text, LV_SYMBOL_REFRESH " RESET");
    else if (feature == UI_FEATURE_ATTITUDE)
        lv_label_set_text(refresh_text, LV_SYMBOL_REFRESH " ZERO");
    else if (feature == UI_FEATURE_STORAGE)
        lv_label_set_text(refresh_text, LV_SYMBOL_REFRESH " RESCAN");
    else
        lv_label_set_text(refresh_text, LV_SYMBOL_REFRESH " REFRESH");
    lv_obj_set_style_text_font(refresh_text, &lv_font_montserrat_12, 0);
    lv_obj_center(refresh_text);

    lv_group_add_obj(group, refresh_button);
    lv_group_focus_obj(refresh_button);
    refresh_status();
    refresh_timer = lv_timer_create(refresh_timer_cb, 500U, NULL);
    return screen;
}
