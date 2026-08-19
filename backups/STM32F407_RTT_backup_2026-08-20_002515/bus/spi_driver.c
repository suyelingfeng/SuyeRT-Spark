/**
 * @file spi_driver.c
 * @brief 通用 SPI 驱动框架实现
 * 
 * 本模块提供与硬件无关的 SPI 总线操作接口，支持多设备共享同一 SPI 总线。
 * 通过设备注册机制和适配层回调实现硬件解耦。
 * 
 * 核心功能：
 * 1. 设备管理：维护打开的设备表，防止重复打开
 * 2. 传输调度：自动管理片选，调用适配层完成实际硬件操作
 * 3. 适配层管理：注册和查询硬件适配层的回调函数
 * 
 * @note 线程安全性：当前实现为非原子操作。多线程环境需外部加锁保护。
 * @note 设备数量限制：最多支持 SPI_MAX_DEVICES 个设备共享一条 SPI 总线。
 * 
 * 数据流：应用代码 → spi_transfer_bytes() → 查表获取适配层回调 → 
 *        硬件适配层(spi2_adapter) → STM32F407 寄存器操作
 */

#include "spi_driver.h"
#include "spi_config.h"
#include <string.h>
#include <rtthread.h>

/* ============================================================================
 * Private Data Structures
 * ========================================================================== */

/**
 * @brief SPI 设备实体结构体（不透明给应用代码）
 * 
 * 应用代码只能通过 spi_device_t 指针访问，无法直接访问内部成员。
 */
struct spi_device
{
    char bus_name[16];           /**< 所属总线的名称（如 "spi2"） */
    uint8_t device_id;           /**< 在总线上的设备编号 */
    bool opened;                 /**< 设备是否已打开 */
};

/**
 * @brief SPI 总线实体结构体
 * 
 * 管理一条 SPI 总线上的所有设备、适配层回调等信息。
 */
typedef struct
{
    char bus_name[16];           /**< 总线名称（如 "spi2"） */
    bool initialized;            /**< 总线硬件是否已初始化 */
    spi_adapter_t adapter;       /**< 适配层回调函数集合 */
    spi_device_t devices[SPI_MAX_DEVICES]; /**< 设备表 */
    uint8_t device_count;        /**< 当前打开的设备数 */
} spi_bus_t;

/* ============================================================================
 * Private Global Variables
 * ========================================================================== */

/** SPI 总线表（支持多条 SPI 总线，如 SPI1、SPI2、SPI3 等） */
static spi_bus_t g_spi_buses[2] = {0};  /* 最多支持 2 条总线 */

/** 已注册的总线数量 */
static uint8_t g_bus_count = 0;

/** 标记 SPI 驱动是否已初始化 */
static bool g_driver_initialized = false;

/* ============================================================================
 * Private Function Declarations
 * ========================================================================== */

/**
 * @brief 按总线名查找 SPI 总线实体
 * @param[in] bus_name 总线名称字符串
 * @retval 非 NULL 找到对应的总线
 * @retval NULL 未找到
 */
static spi_bus_t* spi_bus_find(const char *bus_name);

/**
 * @brief 在指定总线中按设备 ID 查找设备
 * @param[in] bus 总线实体指针
 * @param[in] device_id 设备 ID
 * @retval 非 NULL 找到对应的设备
 * @retval NULL 未找到
 */
static spi_device_t* spi_device_find(spi_bus_t *bus, uint8_t device_id);

/* ============================================================================
 * Public Function Implementations
 * ========================================================================== */

/**
 * @brief 初始化 SPI 驱动框架
 * 
 * 调用所有已注册适配层的 init() 回调，完成硬件初始化。
 * 此函数必须在使用任何其他 SPI API 前调用。
 */
int spi_driver_init(void)
{
    int ret = SPI_OK;
    
    /* 遍历所有已注册的总线 */
    for (uint8_t i = 0; i < g_bus_count; ++i)
    {
        if (!g_spi_buses[i].initialized && g_spi_buses[i].adapter.init != NULL)
        {
            /* 调用适配层的硬件初始化回调 */
            ret = g_spi_buses[i].adapter.init();
            if (ret != SPI_OK)
            {
                rt_kprintf("[SPI] Failed to initialize bus '%s' (ret=%d)\n",
                          g_spi_buses[i].bus_name, ret);
                return ret;
            }
            g_spi_buses[i].initialized = true;
        }
    }
    
    g_driver_initialized = true;
    rt_kprintf("[SPI] Driver initialized successfully\n");
    return SPI_OK;
}

/**
 * @brief 注册 SPI 总线的硬件适配层
 * 
 * 硬件适配层通过本函数将其回调集合注册到通用驱动。
 */
int spi_driver_register_adapter(const char *bus_name, const spi_adapter_t *adapter)
{
    /* 参数检验 */
    if (bus_name == NULL || adapter == NULL)
    {
        rt_kprintf("[SPI] register_adapter: invalid parameters\n");
        return SPI_ERROR_PARAM;
    }
    
    if (g_bus_count >= 2)
    {
        rt_kprintf("[SPI] register_adapter: too many buses (max 2)\n");
        return SPI_ERROR_PARAM;
    }
    
    /* 检查总线是否已注册 */
    if (spi_bus_find(bus_name) != NULL)
    {
        rt_kprintf("[SPI] register_adapter: bus '%s' already registered\n", bus_name);
        return SPI_ERROR_ALREADY_OPEN;
    }
    
    /* 初始化新总线 */
    spi_bus_t *bus = &g_spi_buses[g_bus_count];
    strncpy(bus->bus_name, bus_name, sizeof(bus->bus_name) - 1);
    bus->bus_name[sizeof(bus->bus_name) - 1] = '\0';
    bus->initialized = false;
    bus->adapter = *adapter;
    bus->device_count = 0;
    memset(bus->devices, 0, sizeof(bus->devices));
    
    g_bus_count++;
    rt_kprintf("[SPI] Registered bus '%s'\n", bus_name);
    return SPI_OK;
}

/**
 * @brief 打开指定的 SPI 设备
 * 
 * 返回一个设备句柄，用于后续的传输操作。
 */
spi_device_t* spi_device_open(const char *bus_name, uint8_t device_id)
{
    /* 参数检验 */
    if (bus_name == NULL)
    {
        rt_kprintf("[SPI] device_open: bus_name is NULL\n");
        return NULL;
    }
    
    if (!g_driver_initialized)
    {
        rt_kprintf("[SPI] device_open: driver not initialized\n");
        return NULL;
    }
    
    /* 查找总线 */
    spi_bus_t *bus = spi_bus_find(bus_name);
    if (bus == NULL)
    {
        rt_kprintf("[SPI] device_open: bus '%s' not found\n", bus_name);
        return NULL;
    }
    
    /* 检查设备是否已打开 */
    spi_device_t *dev = spi_device_find(bus, device_id);
    if (dev != NULL && dev->opened)
    {
        rt_kprintf("[SPI] device_open: device %u on bus '%s' already opened\n",
                  device_id, bus_name);
        return NULL;
    }
    
    /* 找一个空的设备槽位 */
    if (dev == NULL)
    {
        if (bus->device_count >= SPI_MAX_DEVICES)
        {
            rt_kprintf("[SPI] device_open: device table full on bus '%s'\n", bus_name);
            return NULL;
        }
        dev = &bus->devices[bus->device_count];
        bus->device_count++;
    }
    
    /* 初始化设备 */
    strncpy(dev->bus_name, bus_name, sizeof(dev->bus_name) - 1);
    dev->bus_name[sizeof(dev->bus_name) - 1] = '\0';
    dev->device_id = device_id;
    dev->opened = true;
    
    rt_kprintf("[SPI] Opened device %u on bus '%s'\n", device_id, bus_name);
    return dev;
}

/**
 * @brief 关闭 SPI 设备
 * 
 * 释放设备句柄占用的资源，允许后续重新打开。
 */
int spi_device_close(spi_device_t *dev)
{
    if (dev == NULL)
    {
        rt_kprintf("[SPI] device_close: dev is NULL\n");
        return SPI_ERROR_PARAM;
    }
    
    dev->opened = false;
    rt_kprintf("[SPI] Closed device %u on bus '%s'\n", dev->device_id, dev->bus_name);
    return SPI_OK;
}

/**
 * @brief 通过指定设备传输指定数量的字节
 * 
 * 自动管理片选，调用适配层的传输函数。
 */
int spi_transfer_bytes(spi_device_t *dev, const uint8_t *tx_buf,
                       uint8_t *rx_buf, uint16_t len)
{
    /* 参数检验 */
    if (dev == NULL)
    {
        rt_kprintf("[SPI] transfer_bytes: dev is NULL\n");
        return SPI_ERROR_PARAM;
    }
    
    if (len == 0)
    {
        rt_kprintf("[SPI] transfer_bytes: len is 0\n");
        return SPI_ERROR_PARAM;
    }
    
    if (!dev->opened)
    {
        rt_kprintf("[SPI] transfer_bytes: device %u not opened\n", dev->device_id);
        return SPI_ERROR_PARAM;
    }
    
    /* 查找总线和适配层 */
    spi_bus_t *bus = spi_bus_find(dev->bus_name);
    if (bus == NULL || bus->adapter.cs_low == NULL || bus->adapter.cs_high == NULL)
    {
        rt_kprintf("[SPI] transfer_bytes: invalid bus adapter\n");
        return SPI_ERROR_DEVICE;
    }
    
    int ret = SPI_OK;
    
    /* 拉低片选 */
    bus->adapter.cs_low(dev->device_id);
    
    /* 尝试使用适配层的块传输回调（如果支持） */
    if (bus->adapter.transfer_bytes != NULL)
    {
        ret = bus->adapter.transfer_bytes(tx_buf, rx_buf, len);
    }
    else
    {
        /* 回退到字节循环传输 */
        if (bus->adapter.transfer_byte == NULL)
        {
            ret = SPI_ERROR_DEVICE;
        }
        else
        {
            for (uint16_t i = 0; i < len; ++i)
            {
                uint8_t tx_byte = (tx_buf != NULL) ? tx_buf[i] : 0xFF;
                uint8_t rx_byte = bus->adapter.transfer_byte(tx_byte);
                if (rx_buf != NULL)
                {
                    rx_buf[i] = rx_byte;
                }
            }
            ret = len;
        }
    }
    
    /* 拉高片选 */
    bus->adapter.cs_high(dev->device_id);
    
    return ret;
}

/**
 * @brief 查询 SPI 设备的打开状态
 */
bool spi_device_is_open(const char *bus_name, uint8_t device_id)
{
    if (bus_name == NULL)
        return false;
    
    spi_bus_t *bus = spi_bus_find(bus_name);
    if (bus == NULL)
        return false;
    
    spi_device_t *dev = spi_device_find(bus, device_id);
    return (dev != NULL && dev->opened);
}

/* ============================================================================
 * Private Function Implementations
 * ========================================================================== */

/**
 * @brief 按总线名查找 SPI 总线实体
 */
static spi_bus_t* spi_bus_find(const char *bus_name)
{
    for (uint8_t i = 0; i < g_bus_count; ++i)
    {
        if (strncmp(g_spi_buses[i].bus_name, bus_name, 16) == 0)
        {
            return &g_spi_buses[i];
        }
    }
    return NULL;
}

/**
 * @brief 在指定总线中按设备 ID 查找设备
 */
static spi_device_t* spi_device_find(spi_bus_t *bus, uint8_t device_id)
{
    if (bus == NULL)
        return NULL;
    
    for (uint8_t i = 0; i < bus->device_count; ++i)
    {
        if (bus->devices[i].device_id == device_id)
        {
            return &bus->devices[i];
        }
    }
    return NULL;
}
