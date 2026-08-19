/**
 * @file ap3216c.h
 * @brief AP3216C 环境光传感器（ALS）驱动。
 *
 * 数据流：soft_i2c 软件 I2C 总线 -> 本驱动读 ALS 数据寄存器
 * -> 换算为 0.1 lux 整数 -> 上层 board_service / 背光调节逻辑。
 */
#ifndef AP3216C_H
#define AP3216C_H

#include "soft_i2c.h"

/** 设备句柄：只记录挂在哪条 soft_i2c 总线上，无运行时状态。 */
typedef struct { const soft_i2c_bus_t *bus; } ap3216c_t;

/**
 * @brief 初始化 AP3216C，仅启用环境光（ALS）通道。
 * @param device 设备句柄，函数内会写入 bus 指针。
 * @param bus    已初始化好的 soft_i2c 总线。
 * @retval true 复位并配置成功；false 参数为空或 I2C 写失败。
 * @note 当前板级策略只启用 ALS，避免 PS 红外 LED 的脉冲电流引起屏幕频闪。
 */
bool ap3216c_init_als_only(ap3216c_t *device, const soft_i2c_bus_t *bus);

/**
 * @brief 读取环境光照度。
 * @param device 设备句柄。
 * @param lux_x10 输出照度，单位 0.1 lux（除以 10 即 lux）。
 * @retval true 读取成功；false 参数为空或 I2C 读失败。
 */
bool ap3216c_read_light(ap3216c_t *device, uint32_t *lux_x10);

#endif
