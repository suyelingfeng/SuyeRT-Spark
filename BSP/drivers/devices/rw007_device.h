/**
 * @file rw007_device.h
 * @brief RW007 Wi-Fi 模块设备驱动头文件
 * 
 * 本模块提供 RW007 Wi-Fi 模块的操作接口，包括复位、启动、帧收发等功能。
 * 通过通用 SPI 驱动框架与硬件通信，不直接操作硬件寄存器。
 * 
 * 功能特性：
 * 1. 硬件复位：通过 GPIO 复位脚重置模块
 * 2. 启动检测：等待模块启动完成
 * 3. 帧收发：完整的 SPI 帧协议支持
 * 4. 中断管理：配置 INT/BUSY 脚为中断输入
 * 5. 诊断功能：检测设备连接状态
 * 
 * RW007 硬件信号线：
 * - SPI_CS（PF10）：片选脚（由通用 SPI 驱动管理）
 * - SPI_CLK（PB13）：时钟脚（由通用 SPI 驱动管理）
 * - SPI_MOSI（PC3）：主机发送脚（由通用 SPI 驱动管理）
 * - SPI_MISO（PC2）：主机接收脚（由通用 SPI 驱动管理）
 * - RST（PG15）：硬件复位脚（GPIO 输出，低有效）
 * - INT/BUSY（PG11）：中断/忙状态脚（GPIO 输入，初始下拉，运行时配置为中断）
 * 
 * 启动流程：
 * 1. 调用 rw007_device_reset() - 拉低复位脚 100ms
 * 2. 拉高复位脚并等待 500ms
 * 3. 调用 rw007_device_wait_startup() - 监测 INT/BUSY 脚，检测启动完成
 * 4. 调用 rw007_device_ready() - 配置 INT/BUSY 为中断模式
 * 
 * 帧协议（RW007 标准）：
 * 帧格式：[LENGTH(2B)] [TYPE(1B)] [RESERVED(1B)] [DATA(LENGTH-2 B)] [CRC32(4B)]
 * - LENGTH：帧长（包含 TYPE、RESERVED、DATA，不含 LENGTH 和 CRC32）
 * - TYPE：帧类型（命令、应答、数据等）
 * - RESERVED：预留字段
 * - DATA：帧数据
 * - CRC32：校验和
 * 
 * @note 本驱动为启动和诊断用途，不提供完整的 Wi-Fi 通信堆栈。
 *       完整的 RW007 驱动应在此基础上扩展。
 * @note 所有操作都是同步阻塞的，不支持异步操作。
 * @note 线程安全性：调用者负责加锁保护并发访问。
 */

#ifndef RW007_DEVICE_H
#define RW007_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Definitions
 * ========================================================================== */

/** RW007 帧头最大长度（字节） */
#define RW007_FRAME_HEADER_SIZE      4

/** RW007 帧 CRC 大小（字节） */
#define RW007_FRAME_CRC_SIZE         4

/** RW007 最大帧大小（字节） */
#define RW007_MAX_FRAME_SIZE         2048

/** RW007 启动检测超时（毫秒） */
#define RW007_STARTUP_TIMEOUT_MS     500

/** RW007 复位脉冲宽度（毫秒） */
#define RW007_RESET_PULSE_MS         100

/** RW007 启动后稳定化延迟（毫秒） */
#define RW007_STABILIZE_DELAY_MS     200

/* ============================================================================
 * Data Types
 * ========================================================================== */

/**
 * @brief RW007 帧结构体
 * 
 * 用于存储 RW007 SPI 帧。
 */
typedef struct
{
    uint16_t length;             /**< 帧长（字节，不含 LENGTH 和 CRC） */
    uint8_t type;                /**< 帧类型 */
    uint8_t reserved;            /**< 预留字段 */
    uint8_t data[RW007_MAX_FRAME_SIZE]; /**< 帧数据 */
    uint32_t crc32;              /**< CRC32 校验和 */
} rw007_frame_t;

/**
 * @brief RW007 设备状态枚举
 */
typedef enum
{
    RW007_STATE_UNINITIALIZED = 0,  /**< 未初始化 */
    RW007_STATE_INITIALIZED = 1,    /**< 已初始化 */
    RW007_STATE_RESETTING = 2,      /**< 复位中 */
    RW007_STATE_STARTING = 3,       /**< 启动中 */
    RW007_STATE_READY = 4           /**< 就绪 */
} rw007_state_t;

/**
 * @brief RW007 设备信息结构体
 */
typedef struct
{
    rw007_state_t state;         /**< 当前设备状态 */
    uint32_t startup_count;      /**< 启动次数 */
    bool has_frames_pending;     /**< 是否有待处理的帧 */
} rw007_device_info_t;

/* ============================================================================
 * Public Function Declarations
 * ========================================================================== */

/**
 * @brief 初始化 RW007 设备驱动
 * 
 * 打开 SPI 设备并初始化 GPIO（复位脚、INT/BUSY 脚）。
 * 此函数必须在使用其他 RW007 API 前调用。
 * 
 * @retval 0 初始化成功
 * @retval 非 0 初始化失败
 * 
 * @note 假设 spi_driver_init() 已成功。
 */
int rw007_device_init(void);

/**
 * @brief 硬件复位 RW007 模块
 * 
 * 通过拉低复位脚 100ms 来复位模块。复位后需等待模块启动完成。
 * 
 * @retval 0 复位成功
 * @retval 非 0 复位失败
 * 
 * @note 必须在 rw007_device_init() 成功后调用。
 * @note 复位后应调用 rw007_device_wait_startup() 检测启动完成。
 * 
 * @example
 * rw007_device_init();
 * rw007_device_reset();
 * rt_thread_mdelay(500);  // 等待启动
 * rw007_device_wait_startup();
 */
int rw007_device_reset(void);

/**
 * @brief 等待 RW007 模块启动完成
 * 
 * 监测 INT/BUSY 脚的状态变化，检测模块启动是否完成。
 * 模块启动完成时 INT/BUSY 脚会产生特定的状态变化。
 * 
 * @retval 0 模块启动完成
 * @retval 非 0 启动超时或失败
 * 
 * @note 必须在 rw007_device_reset() 后调用。
 * @note 本函数为阻塞式，会等待至多 RW007_STARTUP_TIMEOUT_MS 毫秒。
 */
int rw007_device_wait_startup(void);

/**
 * @brief 配置 RW007 模块为就绪状态
 * 
 * 启动完成后调用本函数，将 INT/BUSY 脚从下拉模式切换为中断模式。
 * 之后应配置外部中断以处理 RW007 的异步事件。
 * 
 * @retval 0 配置成功
 * @retval 非 0 配置失败
 * 
 * @note 必须在 rw007_device_wait_startup() 成功后调用。
 */
int rw007_device_ready(void);

/**
 * @brief 发送 SPI 帧到 RW007 模块
 * 
 * 将完整的帧（含帧头、数据、CRC）通过 SPI 发送到模块。
 * 
 * @param[in] frame 帧结构体指针
 * @retval 0 发送成功
 * @retval 非 0 发送失败
 * 
 * @note 帧的 length、type、data 和 crc32 都必须由调用者正确填充。
 */
int rw007_device_send_frame(const rw007_frame_t *frame);

/**
 * @brief 接收 SPI 帧来自 RW007 模块
 * 
 * 从 SPI 接收一个完整的帧，包括帧头、数据、CRC。
 * 
 * @param[out] frame 帧结构体指针，用于返回接收的帧
 * @retval > 0 实际接收的字节数
 * @retval 0 未接收到完整帧
 * @retval < 0 接收失败或 CRC 错误
 * 
 * @note 本函数为阻塞式，会等待完整帧的接收。
 */
int rw007_device_recv_frame(rw007_frame_t *frame);

/**
 * @brief 获取 RW007 设备当前状态
 * 
 * @param[out] info 设备信息结构体指针，用于返回状态信息
 * @retval 0 获取成功
 * @retval 非 0 获取失败
 */
int rw007_device_get_info(rw007_device_info_t *info);

/**
 * @brief 检查 RW007 模块是否就绪
 * 
 * @retval true 模块处于就绪状态
 * @retval false 模块未就绪
 */
bool rw007_device_is_ready(void);

/**
 * @brief 关闭 RW007 设备驱动
 * 
 * 关闭 SPI 设备句柄。
 * 
 * @retval 0 关闭成功
 * @retval 非 0 关闭失败
 */
int rw007_device_close(void);

#ifdef __cplusplus
}
#endif

#endif /* RW007_DEVICE_H */
