/**
 * @file w25q_device.h
 * @brief W25Q Flash 设备驱动头文件
 * 
 * 本模块提供 W25Q 系列 SPI Flash 存储器的操作接口。
 * 通过通用 SPI 驱动框架与硬件通信，不直接操作硬件寄存器。
 * 
 * 功能特性：
 * 1. JEDEC ID 读取：识别 Flash 容量和制造商
 * 2. 状态寄存器查询：检查设备就绪状态
 * 3. 容量计算：根据 JEDEC ID 自动识别容量
 * 4. 初始化检测：验证 Flash 连接和功能
 * 
 * W25Q 系列 JEDEC ID 格式：
 * - 字节 0：制造商 ID（0xEF 表示 Winbond）
 * - 字节 1：设备 ID 高字节（容量相关）
 * - 字节 2：设备 ID 低字节（容量相关）
 * 
 * 容量映射（常见型号）：
 * - 0xEF4014：W25Q80 (1 MB)
 * - 0xEF4015：W25Q16 (2 MB)
 * - 0xEF4016：W25Q32 (4 MB)
 * - 0xEF4017：W25Q64 (8 MB)
 * - 0xEF4018：W25Q128 (16 MB)
 * - 0xEF4019：W25Q256 (32 MB)
 * 
 * @note 本驱动为设备初始化和诊断用途，不提供读写擦除接口。
 *       完整的 W25Q 驱动应在此基础上扩展。
 * @note 所有操作都是同步阻塞的，不支持异步操作和中断。
 * @note 线程安全性：调用者负责加锁保护并发访问。
 */

#ifndef W25Q_DEVICE_H
#define W25Q_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Definitions
 * ========================================================================== */

/** W25Q JEDEC ID 命令（0x9F 后跟 3 字节地址返回 1 字节结果） */
#define W25Q_CMD_JEDEC_ID           0x9F

/** W25Q 读状态寄存器命令 */
#define W25Q_CMD_READ_STATUS_1      0x05

/** W25Q 状态寄存器 1 中的 BUSY 位（Bit 0） */
#define W25Q_STATUS_BUSY            0x01

/** W25Q 制造商 ID（Winbond） */
#define W25Q_MANUFACTURER_ID        0xEF

/* ============================================================================
 * Data Types
 * ========================================================================== */

/**
 * @brief W25Q JEDEC ID 结构体
 * 
 * 用于存储和解析 JEDEC ID。
 */
typedef struct
{
    uint8_t manufacturer;        /**< 制造商 ID（字节 0） */
    uint8_t device_id_high;      /**< 设备 ID 高字节（字节 1） */
    uint8_t device_id_low;       /**< 设备 ID 低字节（字节 2） */
    uint32_t full_id;            /**< 完整 24 位 JEDEC ID（0xFFMMDDLL 格式） */
} w25q_jedec_id_t;

/**
 * @brief W25Q 设备信息结构体
 * 
 * 包含识别后的设备信息。
 */
typedef struct
{
    uint32_t jedec_id;           /**< 完整 JEDEC ID */
    char model_name[16];         /**< 型号名称（如 "W25Q64"） */
    uint32_t capacity_bytes;     /**< 存储容量（字节） */
    uint16_t page_size;          /**< 页大小（字节，通常 256） */
    uint16_t sector_size;        /**< 扇区大小（字节，通常 4096） */
    uint16_t block_size;         /**< 块大小（字节，通常 65536） */
    bool valid;                  /**< 设备信息是否有效 */
} w25q_device_info_t;

/* ============================================================================
 * Public Function Declarations
 * ========================================================================== */

/**
 * @brief 初始化 W25Q 设备驱动
 * 
 * 打开 SPI 设备并验证 Flash 连接。
 * 此函数必须在使用其他 W25Q API 前调用。
 * 
 * @retval 0 初始化成功
 * @retval 非 0 初始化失败
 * 
 * @note 假设 spi_driver_init() 和 spi_device_open("spi2", SPI_DEV_W25Q_ID) 已成功。
 */
int w25q_device_init(void);

/**
 * @brief 读取 W25Q JEDEC ID
 * 
 * 读取并解析 JEDEC ID，用于识别设备型号和容量。
 * 
 * @param[out] jedec_id JEDEC ID 结构体指针，用于返回读取结果
 * @retval 0 读取成功，jedec_id 已填充有效数据
 * @retval 非 0 读取失败
 * 
 * @note 必须在 w25q_device_init() 成功后调用。
 * 
 * @example
 * w25q_jedec_id_t id;
 * if (w25q_device_read_jedec_id(&id) == 0) {
 *     rt_kprintf("Manufacturer: 0x%02X, Device ID: 0x%04X\n",
 *                id.manufacturer, (id.device_id_high << 8) | id.device_id_low);
 * }
 */
int w25q_device_read_jedec_id(w25q_jedec_id_t *jedec_id);

/**
 * @brief 获取 W25Q 设备信息
 * 
 * 根据 JEDEC ID 查表获取设备型号、容量等信息。
 * 
 * @param[in] jedec_id JEDEC ID 结构体指针
 * @param[out] info 设备信息结构体指针，用于返回查询结果
 * @retval 0 查询成功，info 已填充
 * @retval 非 0 查询失败（JEDEC ID 不被识别）
 * 
 * @note 设备信息通过静态查表获得，不涉及 SPI 通信。
 */
int w25q_device_get_info(const w25q_jedec_id_t *jedec_id, w25q_device_info_t *info);

/**
 * @brief 检查 W25Q 设备是否繁忙
 * 
 * 读取设备状态寄存器，检查 BUSY 位。
 * 
 * @retval true 设备繁忙（状态寄存器 BUSY 位为 1）
 * @retval false 设备就绪或读取失败
 * 
 * @note 本函数仅用于诊断和调试，生产环境应改用中断轮询。
 */
bool w25q_device_is_busy(void);

/**
 * @brief 关闭 W25Q 设备驱动
 * 
 * 关闭 SPI 设备句柄。
 * 
 * @retval 0 关闭成功
 * @retval 非 0 关闭失败
 */
int w25q_device_close(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q_DEVICE_H */
