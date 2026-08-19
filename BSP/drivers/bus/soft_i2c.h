/**
 * @file soft_i2c.h
 * @brief GPIO 模拟 I2C 总线公共驱动。
 *
 * 不绑定具体管脚：调用方用 soft_i2c_bus_t 描述任意一组 SCL/SDA；
 * 板上传感器（AHT21/AP3216C/ICM20608）各自实例化自己的总线。
 */
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/** 一条软件 I2C 总线的管脚描述。 */
typedef struct
{
    GPIO_TypeDef *port; /**< SCL/SDA 所在 GPIO 端口（两者须同端口） */
    uint16_t scl_pin;   /**< SCL 管脚号 */
    uint16_t sda_pin;   /**< SDA 管脚号 */
} soft_i2c_bus_t;

/**
 * @brief 使能 DWT 周期计数器，作为微秒级时序基准。
 * @note 全局只需调用一次，且必须先于任何总线事务调用。
 */
void soft_i2c_timebase_init(void);

/**
 * @brief 把总线的 SCL/SDA 配为开漏输出并释放总线（两线置高）。
 * @param bus 总线描述。
 */
void soft_i2c_bus_init(const soft_i2c_bus_t *bus);

/**
 * @brief 向从机写入一段数据（START + 地址/W + 数据 + STOP）。
 * @param bus     总线描述。
 * @param address 7 位从机地址（不含读写位）。
 * @param data    待发送数据缓冲区。
 * @param length  待发送字节数。
 * @retval true  所有字节均收到 ACK。
 * @retval false 任一字节未收到 ACK。
 */
bool soft_i2c_write(const soft_i2c_bus_t *bus, uint8_t address,
                    const uint8_t *data, uint8_t length);

/**
 * @brief 从从机读取一段数据（START + 地址/R + 读数据 + STOP）。
 * @param bus     总线描述。
 * @param address 7 位从机地址（不含读写位）。
 * @param data    接收数据缓冲区。
 * @param length  要读取的字节数。
 * @retval true  地址帧收到 ACK，数据已读入缓冲区。
 * @retval false 从机未应答地址。
 */
bool soft_i2c_read(const soft_i2c_bus_t *bus, uint8_t address,
                   uint8_t *data, uint8_t length);

/**
 * @brief 读取从机寄存器：先写寄存器地址，再重复 START 切换为读。
 * @param bus     总线描述。
 * @param address 7 位从机地址（不含读写位）。
 * @param reg     寄存器地址。
 * @param data    接收数据缓冲区。
 * @param length  要读取的字节数。
 * @retval true  全部阶段均收到 ACK。
 * @retval false 任一阶段未收到 ACK。
 */
bool soft_i2c_read_regs(const soft_i2c_bus_t *bus, uint8_t address,
                        uint8_t reg, uint8_t *data, uint8_t length);

/**
 * @brief 向从机寄存器写入一个字节。
 * @param bus     总线描述。
 * @param address 7 位从机地址（不含读写位）。
 * @param reg     寄存器地址。
 * @param value   要写入的值。
 * @retval true  写入过程均收到 ACK。
 * @retval false 任一字节未收到 ACK。
 */
bool soft_i2c_write_reg(const soft_i2c_bus_t *bus, uint8_t address,
                        uint8_t reg, uint8_t value);

#endif
