/**
 * @file spi_config.h
 * @brief SPI 总线和设备的集中配置管理
 * 
 * 本头文件是 SPI 配置的单一真源（SSOT）。所有 SPI 总线的参数、所有 SPI 设备
 * 的片选 GPIO 映射都集中定义在这里。需要修改 SPI 配置时，只需改这一个文件，
 * 无需在多个驱动文件中反复修改。
 * 
 * 配置内容：
 * 1. SPI 总线参数（波特率、模式、超时等）
 * 2. SPI 设备 ID 定义和片选 GPIO 映射
 * 3. SPI 初始化顺序和依赖关系
 * 
 * @note 本文件由 spi_driver.c、spi2_adapter.c、设备驱动等共同引用。
 *       修改此文件需重新编译整个 SPI 相关模块。
 */

#ifndef SPI_CONFIG_H
#define SPI_CONFIG_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SPI2 Bus Configuration (STM32F407 Hardware)
 * ========================================================================== */

/**
 * @defgroup SPI_BUS_CONFIG SPI 总线参数
 * @{
 */

/** SPI2 总线编号（用于内部映射） */
#define SPI_BUS_SPI2_ID              2

/** SPI2 硬件时钟频率（计算自 APB1 42 MHz，分频系数 4） */
#define SPI_BUS_SPI2_FREQ_HZ         (42000000U / 4)  /* 10.5 MHz */

/** SPI2 通信模式（CPOL=0, CPHA=0 表示时钟空闲低，数据在上升沿采样） */
#define SPI_BUS_SPI2_MODE            0

/** SPI2 数据帧大小（8 位） */
#define SPI_BUS_SPI2_DATA_WIDTH      8

/** @} */

/* ============================================================================
 * SPI Device ID Definitions (在 SPI2 总线上的设备标识符)
 * ========================================================================== */

/**
 * @defgroup SPI_DEVICE_IDS SPI 设备 ID
 * @{
 */

/** W25Q Flash 设备 ID（用于 spi_device_open 的 device_id 参数） */
#define SPI_DEV_W25Q_ID              0

/** RW007 Wi-Fi 模块设备 ID */
#define SPI_DEV_RW007_ID             1

/** @} */

/* ============================================================================
 * W25Q Flash Device GPIO Mapping
 * ========================================================================== */

/**
 * @defgroup W25Q_GPIO_MAP W25Q Flash 片选 GPIO 配置
 * @{
 */

/** W25Q 片选脚端口 */
#define SPI_DEV_W25Q_CS_PORT         GPIOB

/** W25Q 片选脚编号 */
#define SPI_DEV_W25Q_CS_PIN          GPIO_PIN_12

/** W25Q 片选脚上拉/下拉设置（无特殊需求，设置为上拉） */
#define SPI_DEV_W25Q_CS_PULL         GPIO_PULLUP

/** @} */

/* ============================================================================
 * RW007 Wi-Fi Module GPIO Mapping
 * ========================================================================== */

/**
 * @defgroup RW007_GPIO_MAP RW007 Wi-Fi 片选和控制 GPIO 配置
 * @{
 */

/** RW007 片选脚端口 */
#define SPI_DEV_RW007_CS_PORT        GPIOF

/** RW007 片选脚编号 */
#define SPI_DEV_RW007_CS_PIN         GPIO_PIN_10

/** RW007 片选脚上拉/下拉设置 */
#define SPI_DEV_RW007_CS_PULL        GPIO_PULLUP

/** RW007 复位脚端口 */
#define SPI_DEV_RW007_RST_PORT       GPIOG

/** RW007 复位脚编号 */
#define SPI_DEV_RW007_RST_PIN        GPIO_PIN_15

/** RW007 INT/BUSY 脚端口 */
#define SPI_DEV_RW007_INT_PORT       GPIOG

/** RW007 INT/BUSY 脚编号 */
#define SPI_DEV_RW007_INT_PIN        GPIO_PIN_11

/** RW007 初始化阶段 INT/BUSY 脚上拉/下拉设置（初始化时保持确定电平） */
#define SPI_DEV_RW007_INT_INIT_PULL  GPIO_PULLDOWN

/** RW007 就绪后 INT/BUSY 脚上拉/下拉设置（运行时用作中断脚） */
#define SPI_DEV_RW007_INT_READY_PULL GPIO_PULLUP

/** @} */

/* ============================================================================
 * SPI Timing Configuration (Timeouts and Delays)
 * ========================================================================== */

/**
 * @defgroup SPI_TIMING_CONFIG SPI 时序和超时配置
 * @{
 */

/** SPI 字节传输超时计数（CPU 循环次数）*/
#define SPI_TRANSFER_TIMEOUT_COUNT   100000U

/** SPI 字节传输典型耗时（毫秒，用于日志和诊断） */
#define SPI_TRANSFER_TYPICAL_TIME_MS 1

/** RW007 复位脉冲宽度（毫秒，官方建议 >= 100 ms） */
#define RW007_RESET_PULSE_MS         100

/** RW007 启动等待超时（毫秒，官方建议 <= 500 ms） */
#define RW007_STARTUP_TIMEOUT_MS     500

/** RW007 就绪后稳定化延迟（毫秒，给固件初始化留时间） */
#define RW007_STABILIZE_DELAY_MS     200

/** @} */

/* ============================================================================
 * SPI Peripheral Hardware Configuration
 * ========================================================================== */

/**
 * @defgroup SPI_HW_CONFIG SPI 硬件外设配置
 * @{
 */

/** SPI2 硬件外设实例（STM32F407） */
#define SPI_INSTANCE_SPI2            SPI2

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* SPI_CONFIG_H */
