/**
 * @file shell_commands.c
 * @brief RT-Thread MSH commands used for learning and on-board diagnosis.
 */
#include "app_tasks.h"
#include "lv_port_indev.h"
#include "ui_navigation.h"
#include "serial_console_ui.h"
#include "st7789_fsmc.h"

#include "main.h"
#include <finsh.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

/*
 * 姣忔潯涓插彛鍛戒护鐢?tshell 绾跨▼鎵ц銆傚鍑烘牸寮忎负锛? *   static int cmd_xxx(int argc, char **argv) { ... }
 *   MSH_CMD_EXPORT_ALIAS(cmd_xxx, xxx, help text);
 * 娑夊強 LVGL 鐨勫懡浠ゅ彧鑳借皟鐢?app_tasks_request_ui_redraw()/ui_navigation_request_*()
 * 鎻愪氦璇锋眰锛屼笉鑳戒粠 tshell 绾跨▼鐩存帴鍒涘缓鎴栧垹闄?LVGL 瀵硅薄銆? */

static int cmd_logo(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    serial_console_print_logo();
    serial_console_print_quick_help();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_logo, logo, redraw the RT-Spark serial logo);

static int cmd_ui_status(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    rt_kprintf("UI page  : %s / suye\n", ui_navigation_current_name());
    rt_kprintf("LCD      : ST7789V3 %ux%u RGB565\n", ST7789_WIDTH, ST7789_HEIGHT);
    rt_kprintf("LCD ID   : 0x%04X (expected 0x81B3)\n", st7789_get_id());
    rt_kprintf("LCD ready: %s\n", st7789_is_ready() ? "yes" : "no");
    rt_kprintf("Backlight: %s (PF9)\n", st7789_get_backlight() ? "on" : "off");
    rt_kprintf("Keys     : UP/DOWN select RIGHT enter LEFT back\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_ui_status, ui_status, show LCD LVGL and key status);

static int cmd_key_status(int argc, char **argv)
{
    uint32_t mask = lv_port_indev_get_key_mask();
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_kprintf("Raw key mask: 0x%02X\n", mask);
    rt_kprintf("UP=%s DOWN=%s LEFT=%s RIGHT=%s\n",
               (mask & LV_PORT_KEY_UP) ? "pressed" : "released",
               (mask & LV_PORT_KEY_DOWN) ? "pressed" : "released",
               (mask & LV_PORT_KEY_LEFT) ? "pressed" : "released",
               (mask & LV_PORT_KEY_RIGHT) ? "pressed" : "released");
    rt_kprintf("Mapping: UP=previous DOWN=next RIGHT=enter LEFT=back\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_key_status, key_status, show the four physical key states);

static int cmd_ui_page(int argc, char **argv)
{
    if (argc != 2)
    {
        rt_kprintf("Usage: ui_page <home|sensors|environment|attitude|storage|network|system|led|gpio>\n");
        return -1;
    }

    if (strcmp(argv[1], "home") == 0) ui_navigation_request_home();
    else if (strcmp(argv[1], "sensors") == 0) ui_navigation_request_open(UI_FEATURE_SENSORS);
    else if (strcmp(argv[1], "environment") == 0) ui_navigation_request_open(UI_FEATURE_ENVIRONMENT);
    else if (strcmp(argv[1], "attitude") == 0) ui_navigation_request_open(UI_FEATURE_ATTITUDE);
    else if (strcmp(argv[1], "storage") == 0) ui_navigation_request_open(UI_FEATURE_STORAGE);
    else if (strcmp(argv[1], "network") == 0) ui_navigation_request_open(UI_FEATURE_NETWORK);
    else if (strcmp(argv[1], "system") == 0) ui_navigation_request_open(UI_FEATURE_SYSTEM);
    else if (strcmp(argv[1], "led") == 0) ui_navigation_request_open(UI_FEATURE_LED_RINGS);
    else if (strcmp(argv[1], "gpio") == 0) ui_navigation_request_open(UI_FEATURE_GPIO_PINS);
    else
    {
        rt_kprintf("Unknown page. Use home sensors environment attitude storage network system led or gpio.\n");
        return -1;
    }

    rt_kprintf("UI page request submitted: %s\n", argv[1]);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_ui_page, ui_page, switch UI page (home/sensors/environment/attitude/storage/network/system/led/gpio));

static int cmd_led_ring_next(int argc, char **argv)
{
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_request_led_ring_next();
    rt_kprintf("LED ring cycle requested. Run led_ring_status to inspect the result.\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_led_ring_next, led_ring_next, cycle off/center/inner/outer LED ring);

static int cmd_led_ring_status(int argc, char **argv)
{
    static const char *names[] = {"off", "center", "inner", "outer"};
    board_service_snapshot_t s;
    uint8_t mode;
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_get_board_snapshot(&s);
    mode = s.led_ring_mode < 4U ? s.led_ring_mode : 0U;
    rt_kprintf("SK6805: %u pixels, active=%s, DATA=PA7, /OE=PF2\n",
               s.led_ring_pixel_count, names[mode]);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_led_ring_status, led_ring_status, show SK6805 LED ring state);

static int cmd_gpio_status(int argc, char **argv)
{
    board_service_snapshot_t s;
    uint8_t index = 0U;
    if (argc == 2)
    {
        char letter = argv[1][0];
        if ((letter >= 'a') && (letter <= 'i')) letter = (char)(letter - 'a' + 'A');
        if ((letter < 'A') || (letter > 'I') || (argv[1][1] != '\0'))
        {
            rt_kprintf("Usage: gpio_status [A-I]\n");
            return -1;
        }
        index = (uint8_t)(letter - 'A');
    }
    else if (argc != 1)
    {
        rt_kprintf("Usage: gpio_status [A-I]\n");
        return -1;
    }
    app_tasks_get_board_snapshot(&s);
    rt_kprintf("GPIO%c: IDR=0x%04X ODR=0x%04X MODER=0x%08lX\n",
               (char)('A' + index), s.gpio[index].input, s.gpio[index].output,
               (unsigned long)s.gpio[index].mode);
    rt_kprintf("MODER per pin: 0=input 1=output 2=alternate 3=analog.\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_gpio_status, gpio_status, show GPIO port registers (gpio_status [A-I]));

static int cmd_sensor_status(int argc, char **argv)
{
    board_service_snapshot_t s;
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_get_board_snapshot(&s);
    rt_kprintf("sample #%lu\n", (unsigned long)s.sequence);
    rt_kprintf("AHT21    : %s, temp=%d.%d C, humidity=%u.%u %%RH\n",
               s.aht21_ok ? "OK" : "ERROR", s.temperature_x10 / 10,
               s.temperature_x10 < 0 ? -(s.temperature_x10 % 10) : s.temperature_x10 % 10,
               s.humidity_x10 / 10U, s.humidity_x10 % 10U);
    rt_kprintf("AP3216C  : %s, light=%lu.%lu lux, proximity=%u\n",
               s.ap3216c_ok ? "OK" : "ERROR",
               (unsigned long)(s.ambient_light_x10 / 10U),
               (unsigned long)(s.ambient_light_x10 % 10U), s.proximity);
    rt_kprintf("ICM20608 : %s, accel=%d,%d,%d mg, gyro corrected=%d,%d,%d x0.1dps\n",
               s.icm20608_ok ? "OK" : "ERROR",
               s.accel_mg[0], s.accel_mg[1], s.accel_mg[2],
               s.gyro_dps_x10[0], s.gyro_dps_x10[1], s.gyro_dps_x10[2]);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_sensor_status, sensor_status, "read all sensors: AHT21 temp/humidity, AP3216C light/proximity, ICM20608 accel/gyro");

static int cmd_environment_status(int argc, char **argv)
{
    board_service_snapshot_t s;
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_get_board_snapshot(&s);
    rt_kprintf("AHT21 %s, sample #%lu\n", s.aht21_ok ? "OK" : "ERROR",
               (unsigned long)s.sequence);
    rt_kprintf("temperature raw=%d.%d C, Kalman=%d.%d C\n",
               s.temperature_x10 / 10, abs(s.temperature_x10 % 10),
               s.temperature_kalman_x10 / 10,
               abs(s.temperature_kalman_x10 % 10));
    rt_kprintf("humidity    raw=%u.%u %%RH, Kalman=%u.%u %%RH\n",
               s.humidity_x10 / 10U, s.humidity_x10 % 10U,
               s.humidity_kalman_x10 / 10U, s.humidity_kalman_x10 % 10U);
    rt_kprintf("Kalman Q/R: temperature 0.02/0.30, humidity 0.05/0.80\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_environment_status, environment_status, AHT21 raw and Kalman-filtered temperature & humidity);

static int cmd_attitude_status(int argc, char **argv)
{
    board_service_snapshot_t s;
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_get_board_snapshot(&s);
    rt_kprintf("ICM20608: %s, attitude=%s, calibration=%u/%u, ZUPT=%s\n",
               s.icm20608_ok ? "OK" : "ERROR",
               s.attitude_ready ? "READY" : "CALIBRATING",
               s.attitude_calibration_samples,
               s.attitude_calibration_target,
               s.attitude_stationary ? "STILL" : "MOVING");
    rt_kprintf("relative angle: roll=%d.%d pitch=%d.%d yaw=%d.%d deg\n",
               s.roll_x10 / 10, abs(s.roll_x10 % 10),
               s.pitch_x10 / 10, abs(s.pitch_x10 % 10),
               s.yaw_x10 / 10, abs(s.yaw_x10 % 10));
    rt_kprintf("quaternion x10000: w=%d x=%d y=%d z=%d\n",
               s.quaternion_x10000[0], s.quaternion_x10000[1],
               s.quaternion_x10000[2], s.quaternion_x10000[3]);
    rt_kprintf("gyro bias x0.001dps: x=%d y=%d z=%d, IMU temp=%d.%d C\n",
               s.gyro_bias_x1000[0], s.gyro_bias_x1000[1],
               s.gyro_bias_x1000[2], s.imu_temperature_x10 / 10,
               abs(s.imu_temperature_x10 % 10));
    rt_kprintf("gyro corrected x0.1dps: x=%d y=%d z=%d\n",
               s.gyro_dps_x10[0], s.gyro_dps_x10[1], s.gyro_dps_x10[2]);
    rt_kprintf("temp compensation=%s, coeff x0.0001dps/C: x=%d y=%d z=%d\n",
               s.gyro_temperature_compensation_ready ? "LEARNED" : "LEARNING",
               s.gyro_temp_coeff_x10000[0], s.gyro_temp_coeff_x10000[1],
               s.gyro_temp_coeff_x10000[2]);
    rt_kprintf("Note: six-axis IMU has no magnetic yaw reference; yaw is relative and may drift.\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_attitude_status, attitude_status, ICM20608 attitude (quaternion, Euler, bias, gyro calibration));

/* attitude_zero锛氳姹傛妸褰撳墠鏈濆悜璁句负闆剁偣锛涘疄闄呮竻闆跺湪 board_thread 涓畬鎴愩€?*/
static int cmd_attitude_zero(int argc, char **argv)
{
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_request_attitude_zero();
    rt_kprintf("Attitude zero requested. Keep board still and run attitude_status.\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_attitude_zero, attitude_zero, reset attitude to zero (keep board still during calibration));

/* storage_status锛氭墦鍗?W25Q128 鐨?JEDEC ID 涓庡閲忋€丼D 鍗℃彃鍏ユ娴嬶紙PF3锛夈€?*/
static int cmd_storage_status(int argc, char **argv)
{
    board_service_snapshot_t s;
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_get_board_snapshot(&s);
    rt_kprintf("SPI Flash: %s, JEDEC=%02X %02X %02X, capacity=%lu KiB\n",
               s.flash_ok ? "OK" : "ERROR", s.flash_jedec[0], s.flash_jedec[1],
               s.flash_jedec[2], (unsigned long)s.flash_size_kib);
    rt_kprintf("SD card  : %s (detect PF3)\n", s.sd_inserted ? "inserted" : "not inserted");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_storage_status, storage_status, W25Q128 Flash and SD card status);

/* network_status锛氭墦鍗?RW007 鐨勫浣?涓柇寮曡剼鐘舵€侊紱鏈伐绋嬫湭鍚敤瀹屾暣 WLAN 鍗忚鏍堛€?*/
static int cmd_network_status(int argc, char **argv)
{
    board_service_snapshot_t s;
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_get_board_snapshot(&s);
    rt_kprintf("RW007 hardware: %s, RST=%s, INT/BUSY=%s, reset count=%lu\n",
               s.rw007_ready ? "READY" : "BUSY/OFFLINE",
               s.rw007_reset_released ? "released" : "low",
               s.rw007_int_high ? "high" : "low", (unsigned long)s.rw007_reset_count);
    rt_kprintf("Full Wi-Fi requires official RT-Thread SPI/PIN/WLAN/lwIP components.\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_network_status, network_status, RW007 Wi-Fi module hardware status);

/* network_reset锛氳姹傛寜瀹樻柟鏃跺簭澶嶄綅 RW007锛涘疄闄呮媺澶嶄綅鑴氱殑鍔ㄤ綔鍦?board_thread 涓畬鎴愩€?*/
static int cmd_network_reset(int argc, char **argv)
{
    RT_UNUSED(argc); RT_UNUSED(argv);
    app_tasks_request_rw007_reset();
    rt_kprintf("RW007 reset requested; run network_status after one second.\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_network_reset, network_reset, perform RW007 Wi-Fi module reset sequence);

/* backlight锛氭煡璇㈡垨寮€鍏?LCD 鑳屽厜锛圥F9锛岄珮鐢靛钩鐐逛寒锛夈€?*/
static int cmd_backlight(int argc, char **argv)
{
    if (argc == 1)
    {
        rt_kprintf("Backlight is %s. Usage: backlight <on|off>\n",
                   st7789_get_backlight() ? "on" : "off");
        return 0;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        st7789_set_backlight(1);
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        st7789_set_backlight(0);
    }
    else
    {
        rt_kprintf("Usage: backlight <on|off>\n");
        return -1;
    }
    rt_kprintf("Backlight %s.\n", st7789_get_backlight() ? "on" : "off");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_backlight, backlight, control ST7789 LCD backlight (backlight <on|off>));

/* lcd_test锛氭暣灞忓埛绾壊缁曡繃浜?LVGL 缁樺埗娴佺▼锛屼細瑕嗙洊鐣岄潰锛岄渶 'lcd_test ui' 璇锋眰閲嶇粯鎭㈠銆?*/
static int cmd_lcd_test(int argc, char **argv)
{
    uint16_t color;

    if (argc != 2)
    {
        rt_kprintf("Usage: lcd_test <red|green|blue|white|black|ui>\n");
        return -1;
    }

    if (strcmp(argv[1], "ui") == 0)
    {
        app_tasks_request_ui_redraw();
        rt_kprintf("GUI redraw requested.\n");
        return 0;
    }
    if (strcmp(argv[1], "red") == 0) color = 0xF800U;
    else if (strcmp(argv[1], "green") == 0) color = 0x07E0U;
    else if (strcmp(argv[1], "blue") == 0) color = 0x001FU;
    else if (strcmp(argv[1], "white") == 0) color = 0xFFFFU;
    else if (strcmp(argv[1], "black") == 0) color = 0x0000U;
    else
    {
        rt_kprintf("Unknown color. Use red green blue white black or ui.\n");
        return -1;
    }

    st7789_fill_color(color);
    rt_kprintf("LCD filled with %s. Run 'lcd_test ui' to restore the UI.\n", argv[1]);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_lcd_test, lcd_test, lcd_test <red|green|blue|white|black|ui> - WARNING: overwrites UI);

/* reboot锛氬欢鏃?20 ms 绛変覆鍙ｆ彁绀鸿鍙戝嚭鍚庡啀瑙﹀彂 NVIC 绯荤粺澶嶄綅锛岄伩鍏嶆渶鍚庝竴琛屼涪澶便€?*/
static int cmd_reboot(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    rt_kprintf("Rebooting STM32F407...\n");
    rt_thread_mdelay(20);
    NVIC_SystemReset();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_reboot, reboot, reset the MCU);


