/**
 * @file spi_driver.h
 * @brief 通用 SPI 驱动框架 - 与硬件无关的接口定义
 * 
 * 本头文件定义了所有 SPI 驱动的通用接口。设备驱动（如 W25Q、RW007）
 * 通过本头文件提供的 API 与 SPI 总线通信，而无需关心底层硬件实现。
 * 
 * 架构设计：
 * - 通用层（本文件）：定义接口，管理设备表，调度传输
 * - 适配层（spi2_adapter.c）：实现硬件操作，注册回调
 * - 设备驱动层（w25q_device.c、rw007_device.c）：调用通用接口实现特定器件逻辑
 * 
 * 特点：
 * 1. 多设备支持：同一 SPI 总线可挂多个设备，片选自动管理
 * 2. 传输灵活性：支持单字节、多字节、块传输
 * 3. 硬件解耦：底层硬件改变只需改适配层，上层代码不变
 * 4. 可扩展性：DMA、中断等高级功能可通过适配层扩展
 * 
 * @note 线程安全性：当前实现为非原子操作。多线程环境下需由调用者
 *       在上层加互斥锁保护，或在适配层使用硬件原子操作。
 * @note 设备数量限制：最多支持 SPI_MAX_DEVICES 个设备共享一条 SPI 总线。
 * @note 初始化顺序：必须先调用 spi_driver_init()，再调用 spi_device_open()。
 */

#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constant Definitions
 * ========================================================================== */

/** 最多支持的 SPI 设备数量 */
#define SPI_MAX_DEVICES 8

/** 传输超时计数（循环次数，具体时间取决于 CPU 频率） */
#define SPI_TRANSFER_TIMEOUT 100000U

/* ============================================================================
 * Error Code Enumeration
 * ========================================================================== */

/**
 * @brief SPI 驱动错误码
 * 
 * 所有 SPI 操作函数返回此枚举类型的值，用于表示操作结果。
 * 使用统一错误码便于上层统一处理和调试。
 */
typedef enum
{
    SPI_OK = 0,                     /**< 操作成功 */
    SPI_ERROR_PARAM = -1,           /**< 参数错误（NULL 指针、设备 ID 超出范围、传输字节数为 0） */
    SPI_ERROR_DEVICE_NOT_FOUND = -2,/**< 设备未找到（spi_device_open 时指定的设备 ID 不存在） */
    SPI_ERROR_ALREADY_OPEN = -3,    /**< 设备已打开（防止重复打开导致资源泄漏） */
    SPI_ERROR_TIMEOUT = -4,         /**< 传输超时（从 MISO 或 MOSI 等待超时，设备无应答） */
    SPI_ERROR_DEVICE = -5           /**< 设备错误（设备上报的错误状态或硬件故障） */
} spi_error_t;

/* ============================================================================
 * Data Type Definitions
 * ========================================================================== */

/**
 * @brief SPI 模式枚举
 * 
 * SPI 有四种通信模式，由 CPOL（时钟极性）和 CPHA（时钟相位）组合决定。
 * 不同的器件要求不同的模式；本项目使用 SPI_MODE_0。
 */
typedef enum
{
    SPI_MODE_0 = 0,  /**< CPOL=0, CPHA=0：时钟空闲低电平，数据在上升沿采样 */
    SPI_MODE_1 = 1,  /**< CPOL=0, CPHA=1 */
    SPI_MODE_2 = 2,  /**< CPOL=1, CPHA=0 */
    SPI_MODE_3 = 3   /**< CPOL=1, CPHA=1 */
} spi_mode_t;

/**
 * @brief SPI 设备句柄（不透明结构体）
 * 
 * 应用代码通过此句柄与 SPI 设备交互。结构体内部定义在 spi_driver.c 中，
 * 应用代码不应直接访问成员，只需传递给 spi_transfer_bytes() 等函数。
 */
typedef struct spi_device spi_device_t;

/**
 * @brief SPI 硬件适配层回调函数结构体
 * 
 * 每个 SPI 总线（如 SPI2）需要注册一套回调函数，供通用驱动调用以执行实际的硬件操作。
 * 硬件适配层（如 spi2_adapter.c）实现这些回调，并在初始化时通过
 * spi_driver_register_adapter() 注册。
 */
typedef struct
{
    /**
     * @brief 初始化硬件（GPIO、外设寄存器等）
     * @retval 0 成功；非 0 失败
     * 
     * 此回调由 spi_driver_init() 调用一次，确保硬件只初始化一次。
     */
    int (*init)(void);

    /**
     * @brief 为指定设备拉低片选
     * @param device_id 设备 ID（与 spi_device_open 的 device_id 对应）
     * 
     * 实现应根据 device_id 调用对应的 GPIO 拉低操作。
     * 例：device_id == SPI_DEV_W25Q_ID 时拉低 PB12。
     */
    void (*cs_low)(uint8_t device_id);

    /**
     * @brief 为指定设备拉高片选
     * @param device_id 设备 ID
     */
    void (*cs_high)(uint8_t device_id);

    /**
     * @brief 全双工传输一个字节
     * @param value 要发送的字节
     * @retval 同一时钟周期内从 MISO 读回的字节
     * 
     * 实现应：
     * 1. 等待 TXE（发送缓冲空）
     * 2. 将 value 写入 SPI 发送寄存器
     * 3. 等待 RXNE（接收缓冲非空）
     * 4. 从 SPI 接收寄存器读取并返回
     * 
     * 应包含超时保护，防止硬件故障导致永久死等。
     */
    uint8_t (*transfer_byte)(uint8_t value);

    /**
     * @brief 传输多个字节（可选，用于高效块传输）
     * @param tx_buf 发送缓冲区（不为 NULL 时发送其中数据；为 NULL 时发送 0x00）
     * @param rx_buf 接收缓冲区（不为 NULL 时接收数据；为 NULL 时忽略接收）
     * @param len 字节数
     * @retval >= 0: 实际传输的字节数
     * @retval < 0: 错误码
     * 
     * 此回调为可选（可设为 NULL）。如果硬件支持 DMA 或块传输模式，
     * 可在此实现高效传输；否则通用驱动会循环调用 transfer_byte()。
     */
    int (*transfer_bytes)(const uint8_t *tx_buf, uint8_t *rx_buf, uint16_t len);
} spi_adapter_t;

/* ============================================================================
 * Public Function Declarations
 * ========================================================================== */

/**
 * @brief 初始化 SPI 驱动框架
 * 
 * 此函数必须在使用任何其他 SPI 驱动 API 前调用（通常在板级初始化阶段）。
 * 它调用所有已注册适配层的 init() 回调，完成硬件初始化。
 * 
 * @retval SPI_OK 初始化成功
 * @retval 非 0 初始化失败（某个适配层返回错误）
 * 
 * @note 本函数不是线程安全的，应在单线程初始化阶段调用。
 */
int spi_driver_init(void);

/**
 * @brief 注册 SPI 总线的硬件适配层
 * 
 * 硬件适配层（如 spi2_adapter.c）通过本函数将其回调集合注册到通用驱动，
 * 供通用驱动调用以执行硬件操作。
 * 
 * @param[in] bus_name 总线名称字符串（如 "spi2"）；内部复制，不需保留指针
 * @param[in] adapter 适配层回调集合指针（不为 NULL）
 * @retval SPI_OK 注册成功
 * @retval SPI_ERROR_PARAM 参数无效
 * 
 * @note 通常由适配层初始化函数（如 spi2_adapter_init()）调用，
 *       应用代码不应直接调用。
 */
int spi_driver_register_adapter(const char *bus_name, const spi_adapter_t *adapter);

/**
 * @brief 打开指定的 SPI 设备
 * 
 * 此函数返回一个设备句柄，该句柄用于后续的传输操作。同一设备只能打开一次；
 * 重复打开返回 NULL。
 * 
 * @param[in] bus_name 总线名称（如 "spi2"；应与 spi_driver_register_adapter 的名称一致）
 * @param[in] device_id 设备 ID（在同一总线上唯一标识设备，如 SPI_DEV_W25Q_ID）
 * @retval 非 NULL 返回有效的设备句柄
 * @retval NULL 设备打开失败（总线不存在、设备 ID 无效、设备已打开等）
 * 
 * @note 打开前需确保 spi_driver_init() 已调用且设备配置已在 spi_config.h 中定义。
 */
spi_device_t* spi_device_open(const char *bus_name, uint8_t device_id);

/**
 * @brief 关闭 SPI 设备
 * 
 * 释放设备句柄占用的资源，允许后续重新打开设备。
 * 关闭后不应再使用该句柄。
 * 
 * @param[in] dev 设备句柄（由 spi_device_open() 返回）
 * @retval SPI_OK 关闭成功
 * @retval SPI_ERROR_PARAM 句柄无效
 */
int spi_device_close(spi_device_t *dev);

/**
 * @brief 通过指定设备传输指定数量的字节
 * 
 * 这是最常用的 SPI 传输函数。本函数自动管理片选（传输前拉低，传输后拉高），
 * 调用适配层的 transfer_byte() 或 transfer_bytes() 完成实际传输。
 * 
 * @param[in] dev 设备句柄
 * @param[in] tx_buf 发送缓冲区指针
 *        - 不为 NULL 时：发送缓冲区中的数据
 *        - 为 NULL 时：发送全 0x00（用于读操作）
 * @param[out] rx_buf 接收缓冲区指针
 *        - 不为 NULL 时：接收缓冲区用于存放读回的数据
 *        - 为 NULL 时：忽略接收数据（用于写操作）
 * @param[in] len 要传输的字节数（必须 > 0）
 * 
 * @retval >= 0 实际传输的字节数（通常等于 len，除非发生错误）
 * @retval SPI_ERROR_PARAM 参数无效（dev 为 NULL、len 为 0）
 * @retval SPI_ERROR_TIMEOUT 传输超时（底层硬件未按时响应）
 * @retval SPI_ERROR_DEVICE 设备错误（适配层返回的硬件错误）
 * 
 * @note 本函数不提供原子性保证。在多线程环境下，调用者需在上层加互斥锁。
 * @note 片选由本函数自动管理，调用者不应手动操作。
 */
int spi_transfer_bytes(spi_device_t *dev, const uint8_t *tx_buf,
                       uint8_t *rx_buf, uint16_t len);

/**
 * @brief 查询 SPI 设备的打开状态
 * 
 * 用于调试或条件检查，判断指定设备是否已被打开。
 * 
 * @param[in] bus_name 总线名称
 * @param[in] device_id 设备 ID
 * @retval true 设备已打开
 * @retval false 设备未打开或不存在
 */
bool spi_device_is_open(const char *bus_name, uint8_t device_id);

#ifdef __cplusplus
}
#endif

#endif /* SPI_DRIVER_H */
