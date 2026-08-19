/**
 * @file rw007_device.c
 * @brief RW007 Wi-Fi 模块设备驱动实现
 * 
 * 本模块实现 RW007 Wi-Fi 模块的初始化、复位、启动、帧收发等功能。
 * 所有硬件通信都通过通用 SPI 驱动框架进行。
 */

#include "rw007_device.h"
#include "spi_driver.h"
#include "spi_config.h"
#include <rtthread.h>
#include <string.h>

/* ============================================================================
 * Private Data
 * ========================================================================== */

/** RW007 设备句柄（从 spi_device_open 获得） */
static spi_device_t *g_rw007_device = NULL;

/** RW007 设备状态 */
static rw007_device_info_t g_rw007_info = {
    .state = RW007_STATE_UNINITIALIZED,
    .startup_count = 0,
    .has_frames_pending = false
};

/* ============================================================================
 * GPIO Helper Functions (低层 GPIO 操作，与 spi2_adapter 中的实现一致)
 * ========================================================================== */

#define GPIOG_BASE           0x40021800U
#define GPIO_ODR             0x14U
#define GPIO_BSRR            0x18U

/**
 * @brief 设置 GPIO 输出电平（内联函数）
 */
static inline void gpio_set_level(uint32_t port_base, uint8_t pin, uint8_t level)
{
    if (level)
    {
        *(volatile uint32_t *)(port_base + GPIO_BSRR) = (1U << pin);
    }
    else
    {
        *(volatile uint32_t *)(port_base + GPIO_BSRR) = (1U << (pin + 16));
    }
}

/**
 * @brief 读取 GPIO 输入电平
 */
static inline uint8_t gpio_get_level(uint32_t port_base, uint8_t pin)
{
    uint32_t val = *(volatile uint32_t *)(port_base + GPIO_ODR);
    return ((val >> pin) & 1);
}

/* ============================================================================
 * Private Function Declarations
 * ========================================================================== */

/**
 * @brief CRC32 计算（用于帧校验）
 * @param data 数据缓冲区
 * @param len 数据长度
 * @retval CRC32 值
 */
static uint32_t rw007_crc32(const uint8_t *data, uint16_t len);

/* ============================================================================
 * Public Function Implementations
 * ========================================================================== */

/**
 * @brief 初始化 RW007 设备驱动
 */
int rw007_device_init(void)
{
    /* 打开 RW007 设备 */
    g_rw007_device = spi_device_open("spi2", SPI_DEV_RW007_ID);
    if (g_rw007_device == NULL)
    {
        rt_kprintf("[RW007] Failed to open device\n");
        return -1;
    }
    
    /* 初始化 GPIO */
    /* RST 脚（PG15）已在 spi2_adapter 中配置为输出，初始拉低 */
    /* INT/BUSY 脚（PG11）已在 spi2_adapter 中配置为输入，初始下拉 */
    
    g_rw007_info.state = RW007_STATE_INITIALIZED;
    rt_kprintf("[RW007] Device initialized\n");
    return 0;
}

/**
 * @brief 硬件复位 RW007 模块
 */
int rw007_device_reset(void)
{
    if (g_rw007_device == NULL)
    {
        rt_kprintf("[RW007] reset: device not initialized\n");
        return -1;
    }
    
    g_rw007_info.state = RW007_STATE_RESETTING;
    
    /* RST 脚已在初始化时拉低，现在等待足够时间后拉高 */
    rt_kprintf("[RW007] Resetting... ");
    
    /* 确保 RST 脚在低电平至少 100ms */
    rt_thread_mdelay(RW007_RESET_PULSE_MS);
    
    /* 拉高 RST 脚，释放复位 */
    gpio_set_level(SPI_DEV_RW007_RST_PORT, SPI_DEV_RW007_RST_PIN, 1);
    
    rt_kprintf("done\n");
    return 0;
}

/**
 * @brief 等待 RW007 模块启动完成
 */
int rw007_device_wait_startup(void)
{
    if (g_rw007_device == NULL)
    {
        rt_kprintf("[RW007] wait_startup: device not initialized\n");
        return -1;
    }
    
    g_rw007_info.state = RW007_STATE_STARTING;
    
    rt_kprintf("[RW007] Waiting for startup... ");
    
    /* INT/BUSY 脚（PG11）监测：
     * 模块启动时会产生特定的脉冲。
     * 简单的检测方法：等待脉冲结束，INT/BUSY 回到空闲态
     */
    
    uint32_t timeout = RW007_STARTUP_TIMEOUT_MS;
    uint32_t stable_count = 0;
    uint8_t last_level = gpio_get_level(SPI_DEV_RW007_INT_PORT, SPI_DEV_RW007_INT_PIN);
    
    while (timeout > 0)
    {
        rt_thread_mdelay(10);  /* 每 10ms 采样一次 */
        timeout -= 10;
        
        uint8_t current_level = gpio_get_level(SPI_DEV_RW007_INT_PORT, SPI_DEV_RW007_INT_PIN);
        
        if (current_level == last_level)
        {
            stable_count++;
            if (stable_count > 20)  /* 连续 200ms 稳定表示启动完成 */
            {
                rt_kprintf("done\n");
                rt_thread_mdelay(RW007_STABILIZE_DELAY_MS);  /* 额外稳定延迟 */
                g_rw007_info.state = RW007_STATE_READY;
                g_rw007_info.startup_count++;
                return 0;
            }
        }
        else
        {
            stable_count = 0;
            last_level = current_level;
        }
    }
    
    rt_kprintf("timeout\n");
    return -1;
}

/**
 * @brief 配置 RW007 模块为就绪状态
 */
int rw007_device_ready(void)
{
    if (g_rw007_device == NULL)
    {
        rt_kprintf("[RW007] ready: device not initialized\n");
        return -1;
    }
    
    if (g_rw007_info.state != RW007_STATE_READY)
    {
        rt_kprintf("[RW007] ready: device not in READY state\n");
        return -1;
    }
    
    /* 配置 INT/BUSY 脚为中断输入
     * 应用应配置外部中断处理器，在脉冲边沿触发帧接收
     * 这里仅作为占位符，实际中断配置由应用完成
     */
    
    rt_kprintf("[RW007] Device ready for operation\n");
    return 0;
}

/**
 * @brief 发送 SPI 帧到 RW007 模块
 */
int rw007_device_send_frame(const rw007_frame_t *frame)
{
    if (frame == NULL || g_rw007_device == NULL)
    {
        rt_kprintf("[RW007] send_frame: invalid parameters\n");
        return -1;
    }
    
    if (g_rw007_info.state != RW007_STATE_READY)
    {
        rt_kprintf("[RW007] send_frame: device not ready\n");
        return -1;
    }
    
    /* 构造完整帧
     * 格式：[LENGTH(2B)] [TYPE(1B)] [RESERVED(1B)] [DATA(LENGTH-2 B)] [CRC32(4B)]
     */
    
    uint16_t total_size = 2 + frame->length + 4;  /* LENGTH + 帧内容 + CRC32 */
    uint8_t *buf = rt_malloc(total_size);
    if (buf == NULL)
    {
        rt_kprintf("[RW007] send_frame: memory allocation failed\n");
        return -1;
    }
    
    uint16_t offset = 0;
    
    /* 写入 LENGTH（大端） */
    buf[offset++] = (frame->length >> 8) & 0xFF;
    buf[offset++] = frame->length & 0xFF;
    
    /* 写入 TYPE 和 RESERVED */
    buf[offset++] = frame->type;
    buf[offset++] = frame->reserved;
    
    /* 写入 DATA */
    if (frame->length > 2)
    {
        memcpy(&buf[offset], frame->data, frame->length - 2);
        offset += frame->length - 2;
    }
    
    /* 写入 CRC32（大端） */
    buf[offset++] = (frame->crc32 >> 24) & 0xFF;
    buf[offset++] = (frame->crc32 >> 16) & 0xFF;
    buf[offset++] = (frame->crc32 >> 8) & 0xFF;
    buf[offset++] = frame->crc32 & 0xFF;
    
    /* 发送帧 */
    int ret = spi_transfer_bytes(g_rw007_device, buf, NULL, total_size);
    
    rt_free(buf);
    
    if (ret == (int)total_size)
    {
        rt_kprintf("[RW007] Frame sent (%u bytes)\n", total_size);
        return 0;
    }
    else
    {
        rt_kprintf("[RW007] send_frame: transfer failed (ret=%d)\n", ret);
        return -1;
    }
}

/**
 * @brief 接收 SPI 帧来自 RW007 模块
 */
int rw007_device_recv_frame(rw007_frame_t *frame)
{
    if (frame == NULL || g_rw007_device == NULL)
    {
        rt_kprintf("[RW007] recv_frame: invalid parameters\n");
        return -1;
    }
    
    if (g_rw007_info.state != RW007_STATE_READY)
    {
        rt_kprintf("[RW007] recv_frame: device not ready\n");
        return -1;
    }
    
    /* 先接收帧头（4 字节：LENGTH(2) + TYPE(1) + RESERVED(1)） */
    uint8_t header[4] = {0};
    int ret = spi_transfer_bytes(g_rw007_device, NULL, header, 4);
    if (ret != 4)
    {
        rt_kprintf("[RW007] recv_frame: header transfer failed\n");
        return -1;
    }
    
    /* 解析帧长 */
    uint16_t frame_length = ((uint16_t)header[0] << 8) | header[1];
    if (frame_length < 2 || frame_length > RW007_MAX_FRAME_SIZE)
    {
        rt_kprintf("[RW007] recv_frame: invalid frame length (%u)\n", frame_length);
        return -1;
    }
    
    frame->type = header[2];
    frame->reserved = header[3];
    frame->length = frame_length;
    
    /* 接收帧数据和 CRC（共 frame_length + 4 字节） */
    uint8_t *buf = rt_malloc(frame_length + 4);
    if (buf == NULL)
    {
        rt_kprintf("[RW007] recv_frame: memory allocation failed\n");
        return -1;
    }
    
    ret = spi_transfer_bytes(g_rw007_device, NULL, buf, frame_length + 4);
    if (ret != (int)(frame_length + 4))
    {
        rt_kprintf("[RW007] recv_frame: data transfer failed\n");
        rt_free(buf);
        return -1;
    }
    
    /* 复制数据和 CRC */
    memcpy(frame->data, buf, frame_length - 2);
    frame->crc32 = ((uint32_t)buf[frame_length - 2] << 24) |
                   ((uint32_t)buf[frame_length - 1] << 16) |
                   ((uint32_t)buf[frame_length] << 8) |
                   buf[frame_length + 1];
    
    rt_free(buf);
    
    rt_kprintf("[RW007] Frame received (%u bytes)\n", 4 + frame_length + 4);
    return 4 + frame_length + 4;
}

/**
 * @brief 获取 RW007 设备当前状态
 */
int rw007_device_get_info(rw007_device_info_t *info)
{
    if (info == NULL)
    {
        return -1;
    }
    
    *info = g_rw007_info;
    return 0;
}

/**
 * @brief 检查 RW007 模块是否就绪
 */
bool rw007_device_is_ready(void)
{
    return (g_rw007_device != NULL && g_rw007_info.state == RW007_STATE_READY);
}

/**
 * @brief 关闭 RW007 设备驱动
 */
int rw007_device_close(void)
{
    if (g_rw007_device == NULL)
    {
        return -1;
    }
    
    int ret = spi_device_close(g_rw007_device);
    g_rw007_device = NULL;
    g_rw007_info.state = RW007_STATE_UNINITIALIZED;
    
    rt_kprintf("[RW007] Device closed\n");
    return ret;
}

/* ============================================================================
 * Private Function Implementations
 * ========================================================================== */

/**
 * @brief CRC32 计算（多项式 0x04C11DB7）
 */
static uint32_t rw007_crc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    
    for (uint16_t i = 0; i < len; ++i)
    {
        crc ^= ((uint32_t)data[i] << 24);
        
        for (int j = 0; j < 8; ++j)
        {
            crc = (crc & 0x80000000U) ? ((crc << 1) ^ 0x04C11DB7U) : (crc << 1);
        }
    }
    
    return crc;
}
