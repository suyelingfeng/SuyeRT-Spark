/**
 * @file rw007_hw.c
 * @brief RW007 硬件复位、启动等待和 INT/BUSY 读取实现
 *
 * 本模块实现 RW007 的硬件级复位和启动流程。
 * 与通用 SPI 驱动 (spi_driver.h, spi2_adapter.c) 协同工作。
 *
 * GPIO 管脚配置（在 spi2_adapter.c 中完成）：
 * - PG15（RST）：GPIO 输出，初始拉低，用于硬件复位
 * - PG11（INT/BUSY）：GPIO 输入，初始下拉，用于启动检测
 */

#include "rw007_hw.h"
#include "spi_config.h"
#include <rtthread.h>

/* ============================================================================
 * Hardware Abstraction Layer for GPIO Access
 * ========================================================================== */

/** GPIOG 基地址 */
#define GPIOG_BASE              0x40021800U

/** GPIO 输出数据寄存器偏移 */
#define GPIO_ODR                0x14U

/** GPIO 位设置/复位寄存器偏移 */
#define GPIO_BSRR               0x18U

/**
 * @brief 读取 GPIO 输出寄存器
 */
static inline uint32_t gpio_read_odr(uint32_t port_base)
{
    return *(volatile uint32_t *)(port_base + GPIO_ODR);
}

/**
 * @brief 设置 GPIO 脚为高电平（通过 BSRR 的低 16 位）
 */
static inline void gpio_set_high(uint32_t port_base, uint8_t pin)
{
    *(volatile uint32_t *)(port_base + GPIO_BSRR) = (1U << pin);
}

/**
 * @brief 设置 GPIO 脚为低电平（通过 BSRR 的高 16 位）
 */
static inline void gpio_set_low(uint32_t port_base, uint8_t pin)
{
    *(volatile uint32_t *)(port_base + GPIO_BSRR) = (1U << (pin + 16));
}

/**
 * @brief 读取 GPIO 脚的电平
 */
static inline bool gpio_read_pin(uint32_t port_base, uint8_t pin)
{
    uint32_t odr = gpio_read_odr(port_base);
    return ((odr >> pin) & 1U) != 0;
}

/* ============================================================================
 * Public Function Implementations
 * ========================================================================== */

/**
 * @brief 查询 INT/BUSY 管脚电平
 */
void rw007_hw_read_status(rw007_hw_status_t *status)
{
    if (status == RT_NULL)
        return;
    
    /* 读取 INT/BUSY 脚（PG11）的当前电平 */
    status->int_high = gpio_read_pin(GPIOG_BASE, SPI_DEV_RW007_INT_PIN);
    
    /* ready 只置位不清零：模块一旦就绪即视为启动完成，
       之后 INT 电平归 rw007 驱动解释。 */
    if (status->reset_released && status->int_high)
        status->ready = true;
}

/**
 * @brief 按官方时序复位并启动 RW007 模块
 */
void rw007_hw_reset_and_start(rw007_hw_status_t *status)
{
    if (status == RT_NULL)
        return;
    
    rt_kprintf("[RW007_HW] Starting reset and startup sequence...\n");
    
    /* 步骤 1：拉高 CS 脚，复位期间不参与 SPI 总线 */
    gpio_set_high(SPI_DEV_RW007_CS_PORT, SPI_DEV_RW007_CS_PIN);
    
    /* 步骤 2：拉低 RST 脚开始复位 */
    gpio_set_low(SPI_DEV_RW007_RST_PORT, SPI_DEV_RW007_RST_PIN);
    status->reset_released = false;
    status->ready = false;
    
    rt_kprintf("[RW007_HW] RST pulled low, waiting 100 ms for hardware reset...\n");
    rt_thread_mdelay(100U);
    
    /* 步骤 3：释放复位脚（拉高），模块开始启动 */
    gpio_set_high(SPI_DEV_RW007_RST_PORT, SPI_DEV_RW007_RST_PIN);
    status->reset_released = true;
    
    rt_kprintf("[RW007_HW] RST released, waiting for INT/BUSY to go high...\n");
    
    /* 步骤 4：轮询 INT/BUSY 脚，最多等待 500 ms
     * 模块启动时会拉低 INT/BUSY，启动完成后拉高 */
    uint16_t poll_count = 0;
    while (poll_count < 100U)
    {
        if (gpio_read_pin(GPIOG_BASE, SPI_DEV_RW007_INT_PIN))
        {
            status->ready = true;
            rt_kprintf("[RW007_HW] INT/BUSY went high after %d ms\n", poll_count * 5U);
            break;
        }
        rt_thread_mdelay(5U);
        poll_count++;
    }
    
    if (!status->ready)
    {
        rt_kprintf("[RW007_HW] WARNING: Timeout waiting for INT/BUSY (500 ms exceeded)\n");
    }
    
    /* 步骤 5：额外稳定延迟 200 ms，给模块固件留出内部初始化时间 */
    rt_kprintf("[RW007_HW] Stabilization delay 200 ms...\n");
    rt_thread_mdelay(200U);
    
    /* 步骤 6：INT/BUSY 脚已由 spi2_adapter.c 配置为下拉输入
     * 这里仅刷新状态快照，实际的中断配置由应用层完成 */
    rw007_hw_read_status(status);
    
    if (status->ready)
    {
        rt_kprintf("[RW007_HW] RW007 module startup completed successfully\n");
    }
    else
    {
        rt_kprintf("[RW007_HW] WARNING: RW007 module may not be ready\n");
    }
}

