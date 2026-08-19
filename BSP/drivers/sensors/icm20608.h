/**
 * @file icm20608.h
 * @brief ICM20608 六轴 IMU（加速度计 + 陀螺仪 + 温度）驱动。
 *
 * 数据流：soft_i2c 软件 I2C 总线 -> 本驱动读写寄存器 -> 原始值换算为物理量
 * -> icm20608_sample_t 输出给上层 board_service；下游是传感器采样线程。
 */
#ifndef ICM20608_H
#define ICM20608_H

#include "soft_i2c.h"

typedef struct { const soft_i2c_bus_t *bus; } icm20608_t;

/** 一次采样结果，全部换算为物理量/定点数，避免上层再做换算。 */
typedef struct
{
    int16_t accel_mg[3];     /**< 三轴加速度，单位 mg（1/1000 g）。 */
    float gyro_dps[3];       /**< 三轴角速度，单位 °/s。 */
    int16_t temperature_x10; /**< 芯片温度，0.1 摄氏度。 */
} icm20608_sample_t;

/**
 * @brief 初始化 ICM20608：配置时钟源、采样率和量程。
 * @param device 设备句柄，函数内会写入 bus 指针。
 * @param bus    已初始化好的 soft_i2c 总线。
 * @retval true 初始化序列全部写成功；false 参数为空或某次 I2C 写失败。
 */
bool icm20608_init(icm20608_t *device, const soft_i2c_bus_t *bus);

/**
 * @brief 读取一帧完整采样（先校验 WHO_AM_I，再连读 14 字节数据寄存器）。
 * @param device 设备句柄。
 * @param sample 输出采样结果。
 * @retval true 读取并换算成功；false 通信失败或 WHO_AM_I 不匹配（器件掉线）。
 */
bool icm20608_read(icm20608_t *device, icm20608_sample_t *sample);

#endif
