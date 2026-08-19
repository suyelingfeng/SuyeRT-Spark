/**
 * @file rw007_hw.h
 * @brief RW007 模块的硬件复位和启动流程（与通用 SPI 驱动集成）
 *
 * 本模块提供 RW007 硬件级复位和启动流程的辅助函数。
 * SPI 通信由 rw007_device_* 接口处理，本模块只负责 GPIO 复位和启动检测。
 *
 * 数据流：
 * 1. spi_driver_init() 初始化通用 SPI 框架
 * 2. spi2_adapter_register() 注册 STM32F407 SPI2 适配层
 * 3. spi_device_open() 打开 RW007 SPI 设备句柄
 * 4. rw007_hw_reset_and_start() 执行硬件复位和启动
 * 5. rw007_device_init() 初始化设备驱动层
 * 6. rw007_device_* 接口进行通信
 */

#ifndef RW007_HW_H
#define RW007_HW_H

#include <stdbool.h>

/**
 * @brief RW007 启动状态快照
 *
 * 用于追踪 RW007 模块从复位到就绪的状态变化。
 */
typedef struct
{
    bool reset_released; /**< 复位管脚是否已释放（模块开始启动） */
    bool ready;          /**< 模块是否已就绪（INT/BUSY 变高） */
    bool int_high;       /**< 最近一次读到的 INT/BUSY 电平 */
} rw007_hw_status_t;

/**
 * @brief 查询 INT/BUSY 管脚电平
 *
 * 读取 INT/BUSY 脚（PG11）的当前电平，不会释放模块复位。
 * 仅在已释放复位且 INT 为高时置位 ready 标志。
 *
 * @param[inout] status 状态快照指针，int_high 被刷新，满足条件时置位 ready
 *
 * @note 本函数可在启动前后多次调用以检测状态变化。
 */
void rw007_hw_read_status(rw007_hw_status_t *status);

/**
 * @brief 按官方时序复位并启动 RW007 模块
 *
 * 执行 RW007 硬件复位和启动流程：
 * 1. 拉低 RST 脚（PG15）100ms 进行硬件复位
 * 2. 释放 RST 脚，模块开始启动
 * 3. 轮询 INT/BUSY 脚（PG11）最多 500ms，直到变高
 * 4. 额外稳定延迟 200ms，给模块固件初始化时间
 * 5. INT/BUSY 配置从下拉输入改为上拉输入（准备作为中断脚使用）
 *
 * @param[out] status 状态快照指针，返回时 ready 反映模块是否按时就绪
 *
 * @note 时序依据 RT-Thread rw007 软件包对移植层的要求。
 * @note 本函数为阻塞式，会占用线程约 900ms（100 + 500 + 200）。
 * @note 调用前应已完成 spi_driver_init() 和 spi_device_open()。
 *
 * @example
 * spi_driver_init();
 * spi2_adapter_register();
 * spi_device_open("spi2", SPI_DEV_RW007_ID);
 * rw007_hw_status_t status = {0};
 * rw007_hw_reset_and_start(&status);
 * if (status.ready) {
 *     rt_kprintf("RW007 started successfully\n");
 * }
 */
void rw007_hw_reset_and_start(rw007_hw_status_t *status);

#endif /* RW007_HW_H */
