/**
 * @file serial_console_ui.c
 * @brief 适配窄串口终端的 RT-Spark ASCII 启动画面。
 *
 * 每行控制在 64 个 ASCII 字符以内，避免常见串口工具自动换行后破坏图形。
 * 不强制输出 ANSI 颜色码，保证 PuTTY、MobaXterm、XShell 等终端都可读。
 */
#include "serial_console_ui.h"

#include "main.h"
#include "lvgl.h"
#include <rtthread.h>

/**
 * @brief 通过 rt_kprintf 输出 RT-Spark ASCII 启动 Logo 和版本信息。
 * @note 在 gui_thread 启动早期调用；LVGL 版本取自编译期宏，
 *       主频由 HAL_RCC_GetHCLKFreq() 运行时读取，可真实反映时钟配置结果。
 */
void serial_console_print_logo(void)
{
    rt_kprintf("\n");
    rt_kprintf("+----------------------------------------------------------+\n");
    rt_kprintf("|  ____ _____      ____  ____   _    ____  _  __          |\n");
    rt_kprintf("| |  _ \\_   _|    / ___||  _ \\ / \\  |  _ \\| |/ /          |\n");
    rt_kprintf("| | |_) || |_____  \\___ \\| |_) / _ \\ | |_) | ' /           |\n");
    rt_kprintf("| |  _ < | |_____|  ___) |  __/ ___ \\|  _ <| . \\           |\n");
    rt_kprintf("| |_| \\_\\|_|       |____/|_| /_/   \\_\\_| \\_\\_|\\_\\          |\n");
    rt_kprintf("+----------------------------------------------------------+\n");
    rt_kprintf("  RT-Thread 5.2.2 | LVGL %d.%d.%d | STM32F407 @ %lu MHz\n",
               LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
               HAL_RCC_GetHCLKFreq() / 1000000UL);
    rt_kprintf("  Board: RT-Spark / suye | USART1: 115200 8N1\n");
}

/**
 * @brief 输出 MSH 常用命令速查，帮助首次接入串口的用户上手。
 * @note 只列命令名，完整帮助仍以 MSH 内置的 'help' 命令为准。
 */
void serial_console_print_quick_help(void)
{
    rt_kprintf("\n  Type 'help' to list commands. Useful commands:\n");
    rt_kprintf("  ps | free | ui_status | ui_page | key_status | lcd_test\n");
    rt_kprintf("  sensor_status | environment_status | attitude_status | attitude_zero\n");
    rt_kprintf("  storage_status | network_status | network_reset\n\n");
}
