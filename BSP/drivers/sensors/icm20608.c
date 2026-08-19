/**
 * @file icm20608.c
 * @brief ICM20608 初始化、身份检查和物理量换算。
 *
 * 通信细节：走 soft_i2c 软件模拟 I2C，7 位地址 0x68（AD0 接地）；
 * 所有数据寄存器均为大端（MSB 在前）16 位有符号数。
 */
#include "icm20608.h"
#include <rtthread.h>

#define ICM20608_ADDRESS 0x68U  /**< 7 位 I2C 从机地址（AD0 接地时）。 */
#define ICM20608_WHO_AM_I 0xAFU /**< WHO_AM_I(0x75) 期望值，ICM20608 固定为 0xAF。 */

/* 将大端字节序的两个字节拼成 int16_t（传感器寄存器都是 MSB 在前）。 */
static int16_t be16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

/**
 * @brief 初始化 ICM20608：配置时钟源、采样率和量程。
 * @param device 设备句柄，函数内会写入 bus 指针。
 * @param bus    已初始化好的 soft_i2c 总线。
 * @retval true 初始化序列全部写成功；false 参数为空或某次 I2C 写失败。
 * @note 任一步写失败立即返回 false，由上层决定是否重试；
 *       这里不校验 WHO_AM_I，身份检查推迟到 icm20608_read 每次读取时做。
 */
bool icm20608_init(icm20608_t *device, const soft_i2c_bus_t *bus)
{
    if ((device == RT_NULL) || (bus == RT_NULL)) return false;
    device->bus = bus;
    /* 寄存器序列（按顺序写入，含义如下）：
     * 0x6B PWR_MGMT_1   = 0x01：退出睡眠并选择陀螺仪 X 轴 PLL 作时钟源，
     *                           比内部 8 MHz RC 振荡器更稳（手册推荐）；
     *         之后延时 50 ms：PLL 切换需要稳定时间，期间不应继续写寄存器。
     * 0x6C PWR_MGMT_2   = 0x00：六轴全部使能（不 standby 任何一路）。
     * 0x19 SMPLRT_DIV   = 0x13：分频 19+1=20，内部 1 kHz / 20 = 50 Hz 输出速率。
     * 0x1A CONFIG       = 0x03：DLPF_CFG=3，陀螺仪带宽约 41 Hz，抑制振动噪声。
     * 0x1B GYRO_CONFIG  = 0x00：陀螺仪量程 ±250 °/s（对应 131 LSB/(°/s)）。
     * 0x1C ACCEL_CONFIG = 0x00：加速度计量程 ±2 g（对应 16384 LSB/g）。
     * 0x1D ACCEL_CONFIG2= 0x03：加速度计 DLPF 约 44.8 Hz，与陀螺仪带宽匹配。
     */
    /* PLL 时钟、50 Hz 采样、±2 g、±250 dps、约 41/44.8 Hz DLPF。 */
    if (!soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x6BU, 0x01U)) return false;
    rt_thread_mdelay(50U);
    return soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x6CU, 0x00U) &&
           soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x19U, 0x13U) &&
           soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x1AU, 0x03U) &&
           soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x1BU, 0x00U) &&
           soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x1CU, 0x00U) &&
           soft_i2c_write_reg(bus, ICM20608_ADDRESS, 0x1DU, 0x03U);
}

/**
 * @brief 读取一帧完整采样（先校验 WHO_AM_I，再连读 14 字节数据寄存器）。
 * @param device 设备句柄。
 * @param sample 输出采样结果。
 * @retval true 读取并换算成功；false 通信失败或 WHO_AM_I 不匹配（器件掉线）。
 */
bool icm20608_read(icm20608_t *device, icm20608_sample_t *sample)
{
    uint8_t who = 0U;
    uint8_t raw[14];
    if ((device == RT_NULL) || (device->bus == RT_NULL) || (sample == RT_NULL))
        return false;
    /* 每次采样前先读 WHO_AM_I(0x75)：总线受扰或器件掉线时会读到 0xFF 等
     * 假数据，先核对身份能可靠区分"器件掉线"与"数据本身异常"。 */
    if (!soft_i2c_read_regs(device->bus, ICM20608_ADDRESS, 0x75U, &who, 1U) ||
        (who != ICM20608_WHO_AM_I)) return false;
    /* 从 0x3B(ACCEL_XOUT_H) 起连续读 14 字节，一次性取回
     * 加速度 6 字节 + 温度 2 字节 + 陀螺仪 6 字节，保证三者属同一采样时刻。 */
    if (!soft_i2c_read_regs(device->bus, ICM20608_ADDRESS, 0x3BU, raw, sizeof(raw)))
        return false;

    /* 加速度：±2 g 量程下 16384 LSB/g，mg = raw * 1000 / 16384。
     * 先转成 int32_t 再乘 1000，避免 int16_t 溢出。 */
    for (uint8_t i = 0U; i < 3U; ++i)
        sample->accel_mg[i] =
            (int16_t)(((int32_t)be16(&raw[i * 2U]) * 1000) / 16384);
    /* 陀螺仪：±250 °/s 量程下 131 LSB/(°/s)，除以灵敏度即得 °/s。
     * 数据区偏移 8：前 6 字节是加速度、再 2 字节是温度。 */
    for (uint8_t i = 0U; i < 3U; ++i)
        sample->gyro_dps[i] = (float)be16(&raw[8U + i * 2U]) / 131.0f;
    /* 温度：手册公式 T(°C) = raw / 326.8 + 25（灵敏度 326.8≈327 LSB/°C），
     * 输出放大 10 倍：temperature_x10 = 250 + raw * 10 / 327。 */
    sample->temperature_x10 =
        (int16_t)(250 + ((int32_t)be16(&raw[6]) * 10) / 327);
    return true;
}
