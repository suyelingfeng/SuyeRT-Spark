/**
 * @file w25q_device.c
 * @brief W25Q Flash 设备驱动实现
 * 
 * 本模块实现 W25Q Flash 的初始化、识别和基本诊断功能。
 * 所有硬件通信都通过通用 SPI 驱动框架进行。
 */

#include "w25q_device.h"
#include "spi_driver.h"
#include "spi_config.h"
#include <rtthread.h>
#include <string.h>

/* ============================================================================
 * Private Data
 * ========================================================================== */

/** W25Q 设备句柄（从 spi_device_open 获得） */
static spi_device_t *g_w25q_device = NULL;

/**
 * @brief W25Q 型号查表
 * 
 * 根据 JEDEC ID 的高、低字节查询设备型号和容量。
 * 格式：{high_byte, low_byte, 型号名称, 容量字节数}
 */
typedef struct
{
    uint8_t device_id_high;
    uint8_t device_id_low;
    const char *model_name;
    uint32_t capacity_bytes;
    uint16_t page_size;
    uint16_t sector_size;
    uint16_t block_size;
} w25q_model_table_entry_t;

static const w25q_model_table_entry_t g_w25q_model_table[] =
{
    /* {高字节, 低字节, 型号名称, 容量字节, 页大小, 扇区大小, 块大小} */
    {0x40, 0x14, "W25Q80",   1048576,      256, 4096, 65536},   /* 1 MB */
    {0x40, 0x15, "W25Q16",   2097152,      256, 4096, 65536},   /* 2 MB */
    {0x40, 0x16, "W25Q32",   4194304,      256, 4096, 65536},   /* 4 MB */
    {0x40, 0x17, "W25Q64",   8388608,      256, 4096, 65536},   /* 8 MB */
    {0x40, 0x18, "W25Q128",  16777216,     256, 4096, 65536},   /* 16 MB */
    {0x40, 0x19, "W25Q256",  33554432,     256, 4096, 65536},   /* 32 MB */
    /* 更多型号可在此添加 */
};

/** 型号查表项数 */
#define W25Q_MODEL_TABLE_SIZE (sizeof(g_w25q_model_table) / sizeof(g_w25q_model_table[0]))

/* ============================================================================
 * Private Function Declarations
 * ========================================================================== */

/**
 * @brief 在查表中查找 W25Q 型号
 * @param device_id_high 设备 ID 高字节
 * @param device_id_low 设备 ID 低字节
 * @retval 非 NULL 找到对应型号
 * @retval NULL 未找到
 */
static const w25q_model_table_entry_t* w25q_find_model(uint8_t device_id_high,
                                                       uint8_t device_id_low);

/* ============================================================================
 * Public Function Implementations
 * ========================================================================== */

/**
 * @brief 初始化 W25Q 设备驱动
 */
int w25q_device_init(void)
{
    /* 打开 W25Q 设备 */
    g_w25q_device = spi_device_open("spi2", SPI_DEV_W25Q_ID);
    if (g_w25q_device == NULL)
    {
        rt_kprintf("[W25Q] Failed to open device\n");
        return -1;
    }
    
    rt_kprintf("[W25Q] Device initialized\n");
    return 0;
}

/**
 * @brief 读取 W25Q JEDEC ID
 */
int w25q_device_read_jedec_id(w25q_jedec_id_t *jedec_id)
{
    if (jedec_id == NULL)
    {
        rt_kprintf("[W25Q] read_jedec_id: jedec_id is NULL\n");
        return -1;
    }
    
    if (g_w25q_device == NULL)
    {
        rt_kprintf("[W25Q] read_jedec_id: device not initialized\n");
        return -1;
    }
    
    /* 构造 JEDEC ID 读取命令
     * 格式：[命令字节 0x9F] [3 字节哑数据用于读取结果]
     */
    uint8_t tx_buf[] = {0x9F, 0xFF, 0xFF, 0xFF};
    uint8_t rx_buf[4] = {0};
    
    /* 执行 SPI 传输 */
    int ret = spi_transfer_bytes(g_w25q_device, tx_buf, rx_buf, 4);
    if (ret != 4)
    {
        rt_kprintf("[W25Q] read_jedec_id: transfer failed (ret=%d)\n", ret);
        return -1;
    }
    
    /* 解析结果
     * rx_buf[0] 为命令回显（忽略）
     * rx_buf[1] 为制造商 ID
     * rx_buf[2] 为设备 ID 高字节
     * rx_buf[3] 为设备 ID 低字节
     */
    jedec_id->manufacturer = rx_buf[1];
    jedec_id->device_id_high = rx_buf[2];
    jedec_id->device_id_low = rx_buf[3];
    jedec_id->full_id = (rx_buf[1] << 16) | (rx_buf[2] << 8) | rx_buf[3];
    
    rt_kprintf("[W25Q] JEDEC ID: 0x%02X%02X%02X (0x%06X)\n",
              jedec_id->manufacturer, jedec_id->device_id_high, jedec_id->device_id_low,
              jedec_id->full_id);
    
    return 0;
}

/**
 * @brief 获取 W25Q 设备信息
 */
int w25q_device_get_info(const w25q_jedec_id_t *jedec_id, w25q_device_info_t *info)
{
    if (jedec_id == NULL || info == NULL)
    {
        rt_kprintf("[W25Q] get_info: invalid parameters\n");
        return -1;
    }
    
    /* 检查制造商 ID */
    if (jedec_id->manufacturer != W25Q_MANUFACTURER_ID)
    {
        rt_kprintf("[W25Q] get_info: not a Winbond device (0x%02X)\n",
                  jedec_id->manufacturer);
        memset(info, 0, sizeof(w25q_device_info_t));
        info->valid = false;
        return -1;
    }
    
    /* 查表查找型号 */
    const w25q_model_table_entry_t *entry = w25q_find_model(jedec_id->device_id_high,
                                                            jedec_id->device_id_low);
    if (entry == NULL)
    {
        rt_kprintf("[W25Q] get_info: unknown device ID (0x%02X%02X)\n",
                  jedec_id->device_id_high, jedec_id->device_id_low);
        memset(info, 0, sizeof(w25q_device_info_t));
        info->valid = false;
        return -1;
    }
    
    /* 填充设备信息 */
    info->jedec_id = jedec_id->full_id;
    strncpy(info->model_name, entry->model_name, sizeof(info->model_name) - 1);
    info->model_name[sizeof(info->model_name) - 1] = '\0';
    info->capacity_bytes = entry->capacity_bytes;
    info->page_size = entry->page_size;
    info->sector_size = entry->sector_size;
    info->block_size = entry->block_size;
    info->valid = true;
    
    rt_kprintf("[W25Q] Model: %s, Capacity: %u KB\n",
              info->model_name, info->capacity_bytes / 1024);
    
    return 0;
}

/**
 * @brief 检查 W25Q 设备是否繁忙
 */
bool w25q_device_is_busy(void)
{
    if (g_w25q_device == NULL)
    {
        rt_kprintf("[W25Q] is_busy: device not initialized\n");
        return false;
    }
    
    /* 读取状态寄存器命令 */
    uint8_t tx_buf[] = {0x05, 0xFF};  /* 0x05: 读状态寄存器 1，0xFF: 读取 1 字节结果 */
    uint8_t rx_buf[2] = {0};
    
    int ret = spi_transfer_bytes(g_w25q_device, tx_buf, rx_buf, 2);
    if (ret != 2)
    {
        rt_kprintf("[W25Q] is_busy: transfer failed\n");
        return false;
    }
    
    /* rx_buf[1] 为状态寄存器值，Bit 0 为 BUSY 位 */
    bool busy = (rx_buf[1] & W25Q_STATUS_BUSY) != 0;
    return busy;
}

/**
 * @brief 关闭 W25Q 设备驱动
 */
int w25q_device_close(void)
{
    if (g_w25q_device == NULL)
    {
        return -1;
    }
    
    int ret = spi_device_close(g_w25q_device);
    g_w25q_device = NULL;
    
    rt_kprintf("[W25Q] Device closed\n");
    return ret;
}

/* ============================================================================
 * Private Function Implementations
 * ========================================================================== */

/**
 * @brief 在查表中查找 W25Q 型号
 */
static const w25q_model_table_entry_t* w25q_find_model(uint8_t device_id_high,
                                                       uint8_t device_id_low)
{
    for (size_t i = 0; i < W25Q_MODEL_TABLE_SIZE; ++i)
    {
        if (g_w25q_model_table[i].device_id_high == device_id_high &&
            g_w25q_model_table[i].device_id_low == device_id_low)
        {
            return &g_w25q_model_table[i];
        }
    }
    return NULL;
}
