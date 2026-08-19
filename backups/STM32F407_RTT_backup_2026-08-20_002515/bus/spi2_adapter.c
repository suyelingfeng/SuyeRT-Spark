/**
 * @file spi2_adapter.c
 * @brief STM32F407 SPI2 硬件适配层实现
 * 
 * 本模块实现了 STM32F407 SPI2 的硬件操作，包括初始化、片选控制、字节传输。
 * 所有操作均通过寄存器直接进行，不依赖 HAL 库以确保效率。
 */

#include "spi2_adapter.h"
#include "spi_driver.h"
#include "spi_config.h"
#include "stm32f4xx_hal.h"
#include <rtthread.h>

/* ============================================================================
 * Hardware Register Definitions
 * ========================================================================== */

/** SPI2 基地址 */
#define SPI2_BASE            0x40003800U

/** RCC 基地址 */
#define RCC_BASE             0x40023800U

/** GPIOB 基地址 */
#define GPIOB_BASE           0x40020400U

/** GPIOC 基地址 */
#define GPIOC_BASE           0x40020800U

/** GPIOF 基地址 */
#define GPIOF_BASE           0x40021400U

/** GPIOG 基地址 */
#define GPIOG_BASE           0x40021800U

/* SPI 寄存器偏移 */
#define SPI_CR1              0x00U
#define SPI_CR2              0x04U
#define SPI_SR               0x08U
#define SPI_DR               0x0CU

/* RCC 寄存器偏移 */
#define RCC_AHB1ENR          0x30U
#define RCC_APB1ENR          0x40U

/* GPIO 寄存器偏移 */
#define GPIO_MODER           0x00U
#define GPIO_OTYPER          0x04U
#define GPIO_OSPEEDR         0x08U
#define GPIO_PUPDR           0x0CU
#define GPIO_ODR             0x14U
#define GPIO_AFRH            0x24U
#define GPIO_AFRL            0x20U

/* SPI 控制寄存器 1 标志位 */
#define SPI_CR1_CPHA         (1 << 0)
#define SPI_CR1_CPOL         (1 << 1)
#define SPI_CR1_MSTR         (1 << 2)
#define SPI_CR1_BR_Pos       3
#define SPI_CR1_BR_Mask      (7 << SPI_CR1_BR_Pos)
#define SPI_CR1_BR_DIV4      (1 << SPI_CR1_BR_Pos)  /* 分频系数 4 */
#define SPI_CR1_SPE          (1 << 6)
#define SPI_CR1_LSBFIRST     (1 << 7)
#define SPI_CR1_SSI          (1 << 8)
#define SPI_CR1_SSM          (1 << 9)

/* SPI 状态寄存器标志位 */
#define SPI_SR_RXNE          (1 << 0)
#define SPI_SR_TXE           (1 << 1)
#define SPI_SR_BSY           (1 << 7)

/* RCC 使能位 */
#define RCC_APB1ENR_SPI2EN   (1 << 14)
#define RCC_AHB1ENR_GPIOBEN  (1 << 1)
#define RCC_AHB1ENR_GPIOCEN  (1 << 2)
#define RCC_AHB1ENR_GPIOFEN  (1 << 5)
#define RCC_AHB1ENR_GPIOGEN  (1 << 6)

/* ============================================================================
 * Inline Helper Functions for Register Access
 * ========================================================================== */

/**
 * @brief 读取 32 位寄存器
 */
static inline uint32_t reg_read32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

/**
 * @brief 写入 32 位寄存器
 */
static inline void reg_write32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

/**
 * @brief 设置寄存器中的比特位
 */
static inline void reg_set_bits(uint32_t addr, uint32_t mask)
{
    uint32_t val = reg_read32(addr);
    reg_write32(addr, val | mask);
}

/**
 * @brief 清除寄存器中的比特位
 */
static inline void reg_clr_bits(uint32_t addr, uint32_t mask)
{
    uint32_t val = reg_read32(addr);
    reg_write32(addr, val & ~mask);
}

/**
 * @brief 修改寄存器中的特定比特位（先清后设）
 */
static inline void reg_mod_bits(uint32_t addr, uint32_t mask, uint32_t value)
{
    uint32_t val = reg_read32(addr);
    val = (val & ~mask) | (value & mask);
    reg_write32(addr, val);
}

/**
 * @brief 延时（粗略毫秒延时）
 */
static void delay_ms(uint32_t ms)
{
    rt_thread_mdelay(ms);
}

/* ============================================================================
 * GPIO Configuration Functions
 * ========================================================================== */

/**
 * @brief 配置单个 GPIO 脚
 * 
 * @param port_base GPIO 端口基地址（GPIOB_BASE、GPIOC_BASE 等）
 * @param pin GPIO 脚编号（0-15）
 * @param mode GPIO 模式
 *        - 0: 输入
 *        - 1: 通用输出（推挽）
 *        - 2: 复用功能
 *        - 3: 模拟
 * @param otype 输出类型
 *        - 0: 推挽
 *        - 1: 开漏
 * @param ospeed 输出速度
 *        - 0: 低速
 *        - 1: 中速
 *        - 2: 快速
 *        - 3: 高速
 * @param pupd 上拉/下拉
 *        - 0: 无上下拉
 *        - 1: 上拉
 *        - 2: 下拉
 */
static void gpio_config(uint32_t port_base, uint8_t pin, uint8_t mode,
                        uint8_t otype, uint8_t ospeed, uint8_t pupd)
{
    uint32_t mask = (3U << (pin * 2));
    uint32_t value = ((uint32_t)mode << (pin * 2));
    
    /* 配置模式 */
    reg_mod_bits(port_base + GPIO_MODER, mask, value);
    
    /* 配置输出类型 */
    mask = (1U << pin);
    value = ((uint32_t)otype << pin);
    reg_mod_bits(port_base + GPIO_OTYPER, mask, value);
    
    /* 配置输出速度 */
    mask = (3U << (pin * 2));
    value = ((uint32_t)ospeed << (pin * 2));
    reg_mod_bits(port_base + GPIO_OSPEEDR, mask, value);
    
    /* 配置上拉/下拉 */
    mask = (3U << (pin * 2));
    value = ((uint32_t)pupd << (pin * 2));
    reg_mod_bits(port_base + GPIO_PUPDR, mask, value);
}

/**
 * @brief 配置 GPIO 复用功能
 */
static void gpio_set_af(uint32_t port_base, uint8_t pin, uint8_t af)
{
    uint32_t reg_offset = (pin < 8) ? GPIO_AFRL : GPIO_AFRH;
    uint8_t shift = (pin < 8) ? (pin * 4) : ((pin - 8) * 4);
    uint32_t mask = (0xFU << shift);
    uint32_t value = ((uint32_t)af << shift);
    
    reg_mod_bits(port_base + reg_offset, mask, value);
}

/**
 * @brief 设置 GPIO 输出电平
 */
static void gpio_set(uint32_t port_base, uint8_t pin, uint8_t level)
{
    if (level)
    {
        /* 设置为高电平：写 BSR 寄存器低 16 位 */
        reg_write32(port_base + 0x18, (1U << pin));  /* BSRR 寄存器偏移 0x18 */
    }
    else
    {
        /* 设置为低电平：写 BSR 寄存器高 16 位 */
        reg_write32(port_base + 0x18, (1U << (pin + 16)));
    }
}

/* ============================================================================
 * SPI Callback Functions (Adapter Interface)
 * ========================================================================== */

/**
 * @brief SPI2 硬件初始化回调
 * 
 * 实现 spi_adapter_t.init 回调。
 */
static int spi2_hw_init(void)
{
    /* 使能时钟 */
    reg_set_bits(RCC_BASE + RCC_AHB1ENR, RCC_AHB1ENR_GPIOBEN);
    reg_set_bits(RCC_BASE + RCC_AHB1ENR, RCC_AHB1ENR_GPIOCEN);
    reg_set_bits(RCC_BASE + RCC_AHB1ENR, RCC_AHB1ENR_GPIOFEN);
    reg_set_bits(RCC_BASE + RCC_AHB1ENR, RCC_AHB1ENR_GPIOGEN);
    reg_set_bits(RCC_BASE + RCC_APB1ENR, RCC_APB1ENR_SPI2EN);
    
    /* 配置 SPI2 引脚 */
    /* PB13: SCK (AF5, 推挽, 快速, 上拉) */
    gpio_config(GPIOB_BASE, 13, 2, 0, 2, 1);  /* mode=2(AF), otype=0, ospeed=2, pupd=1(pull-up) */
    gpio_set_af(GPIOB_BASE, 13, 5);
    
    /* PC2: MISO (AF5, 推挽, 快速, 上拉) */
    gpio_config(GPIOC_BASE, 2, 2, 0, 2, 1);
    gpio_set_af(GPIOC_BASE, 2, 5);
    
    /* PC3: MOSI (AF5, 推挽, 快速, 上拉) */
    gpio_config(GPIOC_BASE, 3, 2, 0, 2, 1);
    gpio_set_af(GPIOC_BASE, 3, 5);
    
    /* PB12: W25Q CS (GPIO输出, 推挽, 快速, 上拉, 初始高) */
    gpio_config(GPIOB_BASE, 12, 1, 0, 2, 1);  /* mode=1(output) */
    gpio_set(GPIOB_BASE, 12, 1);  /* 初始拉高 */
    
    /* PF10: RW007 CS (GPIO输出, 推挽, 快速, 上拉, 初始高) */
    gpio_config(GPIOF_BASE, 10, 1, 0, 2, 1);
    gpio_set(GPIOF_BASE, 10, 1);
    
    /* PG15: RW007 RST (GPIO输出, 推挽, 快速, 无上下拉, 初始低) */
    gpio_config(GPIOG_BASE, 15, 1, 0, 2, 0);
    gpio_set(GPIOG_BASE, 15, 0);  /* 初始拉低（复位态） */
    
    /* PG11: RW007 INT/BUSY (GPIO输入, 下拉) */
    gpio_config(GPIOG_BASE, 11, 0, 0, 0, 2);  /* mode=0(input), pupd=2(pull-down) */
    
    /* 初始化 SPI2 寄存器 */
    uint32_t cr1 = SPI_CR1_MSTR       /* 主机模式 */
                 | SPI_CR1_BR_DIV4    /* 分频系数 4 (10.5 MHz) */
                 | SPI_CR1_SSM        /* 软件片选管理 */
                 | SPI_CR1_SSI;       /* 内部片选由软件设置为高 */
    
    reg_write32(SPI2_BASE + SPI_CR1, cr1);
    
    /* CR2 设置为 0（无中断、无 DMA） */
    reg_write32(SPI2_BASE + SPI_CR2, 0);
    
    /* 使能 SPI2 */
    reg_set_bits(SPI2_BASE + SPI_CR1, SPI_CR1_SPE);
    
    rt_kprintf("[SPI2] Hardware initialized\n");
    return 0;
}

/**
 * @brief 片选低电平回调
 * 
 * 实现 spi_adapter_t.cs_low 回调。
 */
static void spi2_cs_low(uint8_t device_id)
{
    if (device_id == SPI_DEV_W25Q_ID)
    {
        gpio_set(GPIOB_BASE, 12, 0);  /* PB12 拉低 */
    }
    else if (device_id == SPI_DEV_RW007_ID)
    {
        gpio_set(GPIOF_BASE, 10, 0);  /* PF10 拉低 */
    }
}

/**
 * @brief 片选高电平回调
 * 
 * 实现 spi_adapter_t.cs_high 回调。
 */
static void spi2_cs_high(uint8_t device_id)
{
    if (device_id == SPI_DEV_W25Q_ID)
    {
        gpio_set(GPIOB_BASE, 12, 1);  /* PB12 拉高 */
    }
    else if (device_id == SPI_DEV_RW007_ID)
    {
        gpio_set(GPIOF_BASE, 10, 1);  /* PF10 拉高 */
    }
}

/**
 * @brief 单字节全双工传输回调
 * 
 * 实现 spi_adapter_t.transfer_byte 回调。
 * 包含超时保护。
 */
static uint8_t spi2_transfer_byte(uint8_t value)
{
    uint32_t timeout;
    
    /* 等待 TXE（发送缓冲空） */
    timeout = SPI_TRANSFER_TIMEOUT;
    while ((reg_read32(SPI2_BASE + SPI_SR) & SPI_SR_TXE) == 0)
    {
        if (--timeout == 0)
            return 0xFF;  /* 超时返回 0xFF */
    }
    
    /* 写入数据 */
    reg_write32(SPI2_BASE + SPI_DR, value);
    
    /* 等待 RXNE（接收缓冲非空） */
    timeout = SPI_TRANSFER_TIMEOUT;
    while ((reg_read32(SPI2_BASE + SPI_SR) & SPI_SR_RXNE) == 0)
    {
        if (--timeout == 0)
            return 0xFF;
    }
    
    /* 读取数据 */
    uint8_t result = (uint8_t)reg_read32(SPI2_BASE + SPI_DR);
    return result;
}

/**
 * @brief 块传输回调（可选）
 * 
 * 实现 spi_adapter_t.transfer_bytes 回调（可选）。
 * 如果硬件不支持块传输，可设为 NULL，通用驱动会自动循环调用 transfer_byte()。
 * 本实现返回 NULL 表示使用循环字节传输。
 */
static int spi2_transfer_bytes(const uint8_t *tx_buf, uint8_t *rx_buf, uint16_t len)
{
    /* 本实现为空，由通用驱动循环调用 spi2_transfer_byte() */
    (void)tx_buf;
    (void)rx_buf;
    (void)len;
    return -1;  /* 表示不支持，让通用驱动使用字节循环 */
}

/* ============================================================================
 * Public Function Implementations
 * ========================================================================== */

/**
 * @brief 初始化 SPI2 适配层
 */
int spi2_adapter_init(void)
{
    /* 调用硬件初始化 */
    int ret = spi2_hw_init();
    if (ret != 0)
    {
        rt_kprintf("[SPI2] spi2_hw_init failed\n");
        return ret;
    }
    
    /* 注册到通用驱动 */
    ret = spi2_adapter_register();
    if (ret != 0)
    {
        rt_kprintf("[SPI2] spi2_adapter_register failed\n");
        return ret;
    }
    
    return 0;
}

/**
 * @brief 注册 SPI2 适配层回调
 */
int spi2_adapter_register(void)
{
    /* 构造适配层回调结构体 */
    static spi_adapter_t adapter = {
        .init = spi2_hw_init,
        .cs_low = spi2_cs_low,
        .cs_high = spi2_cs_high,
        .transfer_byte = spi2_transfer_byte,
        .transfer_bytes = NULL  /* 不支持块传输，由通用驱动循环调用 transfer_byte */
    };
    
    /* 注册到通用驱动框架 */
    int ret = spi_driver_register_adapter("spi2", &adapter);
    if (ret != 0)
    {
        rt_kprintf("[SPI2] Failed to register adapter\n");
        return ret;
    }
    
    rt_kprintf("[SPI2] Adapter registered\n");
    return 0;
}
