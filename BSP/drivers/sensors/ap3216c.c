/**
 * @file ap3216c.c
 * @brief AP3216C 寄存器访问与照度换算。
 *
 * 通信细节：走 soft_i2c 软件模拟 I2C，7 位地址 0x1E；
 * ALS 数据为 16 位无符号数，寄存器 0x0C 低字节在前（小端）。
 */
#include "ap3216c.h"
#include <rtthread.h>

#define AP3216C_ADDRESS 0x1EU /**< 7 位 I2C 从机地址（固定）。 */

/**
 * @brief 初始化 AP3216C，仅启用环境光（ALS）通道。
 * @param device 设备句柄，函数内会写入 bus 指针。
 * @param bus    已初始化好的 soft_i2c 总线。
 * @retval true 复位并配置成功；false 参数为空或 I2C 写失败。
 */
bool ap3216c_init_als_only(ap3216c_t *device, const soft_i2c_bus_t *bus)
{
    if ((device == RT_NULL) || (bus == RT_NULL)) return false;
    device->bus = bus;
    /* 系统寄存器 0x00 = 0x04：软件复位。复位后芯片需要一段启动时间才能
     * 接受新配置，延时 15 ms 再写模式字，否则配置可能被复位流程吞掉。 */
    if (!soft_i2c_write_reg(bus, AP3216C_ADDRESS, 0x00U, 0x04U)) return false;
    rt_thread_mdelay(15U);
    /* 0x01=仅 ALS；0x03 会同时启用 PS/IR 发射。 */
    return soft_i2c_write_reg(bus, AP3216C_ADDRESS, 0x00U, 0x01U);
}

/**
 * @brief 读取环境光照度。
 * @param device 设备句柄。
 * @param lux_x10 输出照度，单位 0.1 lux（除以 10 即 lux）。
 * @retval true 读取成功；false 参数为空或 I2C 读失败。
 */
bool ap3216c_read_light(ap3216c_t *device, uint32_t *lux_x10)
{
    uint8_t raw[2];
    uint16_t als;
    if ((device == RT_NULL) || (device->bus == RT_NULL) || (lux_x10 == RT_NULL))
        return false;
    /* ALS 数据寄存器 0x0C(低字节)/0x0D(高字节)，从 0x0C 起连读 2 字节。 */
    if (!soft_i2c_read_regs(device->bus, AP3216C_ADDRESS, 0x0CU, raw, sizeof(raw)))
        return false;
    /* 注意：AP3216C 的 ALS 数据是小端（低字节在前），与 ICM20608 的大端相反。 */
    als = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8U);
    *lux_x10 = ((uint32_t)als * 35U) / 10U; /* 默认量程：0.35 lux/LSB。 */
    return true;
}
