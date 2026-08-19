/**
 * @file spi2_adapter.h
 * @brief STM32F407 SPI2 硬件适配层头文件
 * 
 * 本模块将 STM32F407 的 SPI2 硬件操作适配到通用 SPI 驱动框架。
 * 它实现了 spi_driver.h 中定义的 spi_adapter_t 回调接口。
 * 
 * 职责：
 * 1. 初始化 SPI2 硬件（GPIO、外设寄存器配置）
 * 2. 管理所有设备的片选 GPIO（拉低/拉高）
 * 3. 实现单字节全双工传输
 * 4. 提供超时保护防止硬件故障导致死等
 * 
 * 硬件配置：
 * - SPI2 波特率：10.5 MHz（fPCLK=42MHz / 4）
 * - 数据帧：8 位
 * - 模式：SPI Mode 0（CPOL=0, CPHA=0）
 * - 片选：软件管理（GPIO）
 * 
 * GPIO 映射：
 * - PB13：SCK（AF5）
 * - PC2：MISO（AF5）
 * - PC3：MOSI（AF5）
 * - PB12：W25Q CS（GPIO 输出，上拉）
 * - PF10：RW007 CS（GPIO 输出，上拉）
 * - PG15：RW007 RST（GPIO 输出，上拉）
 * - PG11：RW007 INT/BUSY（GPIO 输入，初始下拉，就绪后上拉）
 * 
 * @note 本模块仅负责硬件初始化和寄存器操作，不处理设备逻辑。
 * @note 所有函数都是 HAL 无关的寄存器直接操作，确保运行效率。
 */

#ifndef SPI2_ADAPTER_H
#define SPI2_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SPI2 硬件适配层
 * 
 * 此函数完成以下工作：
 * 1. 使能 SPI2、GPIO 的时钟
 * 2. 配置 SCK/MISO/MOSI 为复用功能（AF5）
 * 3. 配置片选脚（PB12、PF10）为推挽输出，初始拉高
 * 4. 配置 RW007 复位脚（PG15）为推挽输出，初始拉低（复位态）
 * 5. 配置 RW007 INT/BUSY 脚（PG11）为下拉输入（启动阶段）
 * 6. 配置 SD 检测脚（PF3）为上拉输入
 * 7. 初始化 SPI2 寄存器（CR1/CR2）
 * 8. 注册到通用驱动框架
 * 
 * @retval 0 初始化成功
 * @retval 非 0 初始化失败
 * 
 * @note 必须在 spi_driver_init() 前调用，通常在板级初始化（board_init）中调用。
 */
int spi2_adapter_init(void);

/**
 * @brief 将 SPI2 适配层注册到通用驱动框架
 * 
 * 此函数由 spi2_adapter_init() 内部调用，无需应用代码显式调用。
 * 它向 spi_driver 注册 SPI2 的回调函数集合。
 * 
 * @retval 0 注册成功
 * @retval 非 0 注册失败
 */
int spi2_adapter_register(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI2_ADAPTER_H */
