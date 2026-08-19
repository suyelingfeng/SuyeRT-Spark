/**
 * @file aht21.c
 * @brief AHT21 命令、时序和原始数据换算。
 *
 * 通信细节：走 soft_i2c 软件模拟 I2C，7 位地址 0x38；命令直接整帧写入
 * （无寄存器地址概念），读取时一次读回 6 字节：状态 + 20 位湿度 + 20 位温度。
 */
#include "aht21.h"

#define AHT21_ADDRESS       0x38U /**< 7 位 I2C 从机地址（固定）。 */
#define AHT21_CONVERSION_MS 85U   /**< 转换等待时间；手册标称最大 80 ms，留 5 ms 余量。 */

/**
 * @brief 初始化器件；调用线程允许短暂休眠 20 ms。
 * @param device 设备句柄，函数内会写入 bus 指针并清空转换状态。
 * @param bus    已初始化好的 soft_i2c 总线。
 * @retval true 初始化命令发出并等待完成；false 参数为空或 I2C 写失败。
 */
bool aht21_init(aht21_t *device, const soft_i2c_bus_t *bus)
{
    /* 0xBE 初始化命令（0x08 为使能校准的参数）：上电后必须先执行，
     * 否则传感器处于未校准状态，读数无效。 */
    static const uint8_t init_command[] = {0xBEU, 0x08U, 0x00U};
    if ((device == RT_NULL) || (bus == RT_NULL)) return false;
    device->bus = bus;
    device->measurement_pending = false;
    device->measurement_started = 0U;
    if (!soft_i2c_write(bus, AHT21_ADDRESS, init_command, sizeof(init_command)))
        return false;
    /* 手册要求初始化后等待 20 ms 才能开始第一次测量。 */
    rt_thread_mdelay(20U);
    return true;
}

/**
 * @brief 发起一次转换，不等待测量完成。
 * @param device 设备句柄。
 * @retval true 触发命令发送成功并记录起始节拍；false 参数为空或 I2C 写失败。
 * @note 发送失败时清掉 pending 标志，防止上层 poll 到一次根本没发起的转换。
 */
bool aht21_start(aht21_t *device)
{
    /* 0xAC 触发测量命令，0x33/0x00 是手册规定的固定参数。 */
    static const uint8_t command[] = {0xACU, 0x33U, 0x00U};
    if ((device == RT_NULL) || (device->bus == RT_NULL)) return false;
    if (!soft_i2c_write(device->bus, AHT21_ADDRESS, command, sizeof(command)))
    {
        device->measurement_pending = false;
        return false;
    }
    device->measurement_pending = true;
    device->measurement_started = rt_tick_get();
    return true;
}

/**
 * @brief 轮询读取转换结果（非阻塞）。
 * @param device 设备句柄。
 * @param sample 输出温湿度，仅在返回 1 时写入。
 * @retval 1  取得新数据。
 * @retval 0  无在途转换，或距发起不足 AHT21_CONVERSION_MS，稍后再调。
 * @retval -1 I2C 读失败，或状态字 bit7(忙) 仍为 1（器件尚未转完，属异常）。
 */
int aht21_poll(aht21_t *device, aht21_sample_t *sample)
{
    uint8_t raw[6];
    uint32_t humidity;
    uint32_t temperature;
    if ((device == RT_NULL) || (sample == RT_NULL) || !device->measurement_pending)
        return 0;
    /* 用 rt_tick 差值判断 85 ms 是否到期：节拍回绕时无符号减法仍然成立。 */
    if ((rt_tick_get() - device->measurement_started) <
        rt_tick_from_millisecond(AHT21_CONVERSION_MS)) return 0;

    /* 先清 pending 再读总线：读失败返回 -1 后不会残留挂起状态，
     * 上层可安全地直接 start 下一次转换。 */
    device->measurement_pending = false;
    if (!soft_i2c_read(device->bus, AHT21_ADDRESS, raw, sizeof(raw)) ||
        ((raw[0] & 0x80U) != 0U)) return -1;

    /* 6 字节布局：raw[0]=状态；湿度和温度各 20 位、共享 raw[3] 的高低半字节。
     * 湿度 = raw[1]<<12 | raw[2]<<4 | raw[3]>>4；
     * 温度 = (raw[3]&0x0F)<<16 | raw[4]<<8 | raw[5]。 */
    humidity = ((uint32_t)raw[1] << 12U) |
               ((uint32_t)raw[2] << 4U) | (raw[3] >> 4U);
    temperature = ((uint32_t)(raw[3] & 0x0FU) << 16U) |
                  ((uint32_t)raw[4] << 8U) | raw[5];
    /* 换算公式（手册）：RH% = raw / 2^20 * 100，T(°C) = raw / 2^20 * 200 - 50。
     * 输出放大 10 倍并以 >>20 代替除法：humidity_x10 = raw * 1000 / 2^20，
     * temperature_x10 = raw * 2000 / 2^20 - 500。 */
    sample->humidity_x10 = (uint16_t)((humidity * 1000U) >> 20U);
    sample->temperature_x10 =
        (int16_t)(((int32_t)(temperature * 2000U) >> 20U) - 500);
    return 1;
}

bool aht21_is_pending(const aht21_t *device)
{
    return (device != RT_NULL) && device->measurement_pending;
}
