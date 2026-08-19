/**
 * @file aht21.h
 * @brief AHT21 温湿度传感器驱动（非阻塞转换）。
 *
 * 数据流：上层先 aht21_start() 发起转换，85 ms 后周期性 aht21_poll()
 * 取回温湿度；start/poll 分离是为了让传感器线程在转换期间能处理其他事务，
 * 而不是原地空等。
 */
#ifndef AHT21_H
#define AHT21_H

#include "soft_i2c.h"
#include <rtthread.h>

/** 设备句柄：bus 指针 + 非阻塞转换状态字。状态未加锁，须由单一线程驱动。 */
typedef struct
{
    const soft_i2c_bus_t *bus;
    bool measurement_pending;      /**< true=已发起转换、尚未读取结果。 */
    rt_tick_t measurement_started; /**< 发起转换时的系统节拍，用于判断 85 ms 是否到期。 */
} aht21_t;

typedef struct
{
    int16_t temperature_x10;  /* 0.1 摄氏度。 */
    uint16_t humidity_x10;    /* 0.1 %RH。 */
} aht21_sample_t;

/** 初始化器件；调用线程允许短暂休眠 20 ms。 */
bool aht21_init(aht21_t *device, const soft_i2c_bus_t *bus);
/** 发起一次转换，不等待测量完成。 */
bool aht21_start(aht21_t *device);
/** 返回 1=取得新数据，0=尚未到期，-1=通信或数据错误。 */
int aht21_poll(aht21_t *device, aht21_sample_t *sample);
/** 查询当前是否有尚未读取的转换在途。 */
bool aht21_is_pending(const aht21_t *device);

#endif
