/**
 * @file st7789_fsmc.c
 * @brief 8-bit FSMC bus driver for the on-board ST7789V3 LCD.
 *
 * The schematic connects the display to FSMC bank 3.  Address line A18 is
 * used as D/C: address 0x6803FFFE writes a command and 0x68040000 writes data.
 *
 * 数据流：LVGL flush 回调 -> st7789_blit() -> 本驱动的命令/数据写入；
 * MSH 诊断命令 -> st7789_fill_color()/st7789_set_backlight()。
 * 上游 FSMC/GPIO 由 HAL_SRAM_MspInit() 与 lcd_control_gpio_init() 配置。
 */
#include "st7789_fsmc.h"

#include "main.h"
#include "stm32f4xx_hal_sram.h"
#include <rtthread.h>

/* FSMC bank3(NE3) 片选 0x68000000 起的地址空间；原理图用地址线 A18 充当屏的
 * D/C(RS)：A18=0 的地址写命令，A18=1（基地址 + 0x40000）的地址写数据。
 * NE3 片选、NWE/NOE 读写时序由 FSMC 自动生成，软件只做一次内存访问即可。 */
#define LCD_COMMAND_ADDRESS ((volatile uint8_t *)0x6803FFFEUL)
#define LCD_DATA_ADDRESS    ((volatile uint8_t *)0x68040000UL)

/* Named locally because the CubeMX project intentionally leaves LCD pins in BSP. */
/* 复位 PD3、背光 PF9 是普通 GPIO，不属于 FSMC 复用脚。 */
#define LCD_RESET_GPIO GPIOD
#define LCD_RESET_PIN  GPIO_PIN_3
#define LCD_BL_GPIO    GPIOF
#define LCD_BL_PIN     GPIO_PIN_9

/* lcd_bus_mutex 保护 LVGL 刷新线程与 MSH 诊断命令对总线的并发访问；
 * lcd_mutex_ready/lcd_ready 标记初始化阶段，避免过早加锁或访问未就绪硬件。 */
static SRAM_HandleTypeDef lcd_sram;
static struct rt_mutex lcd_bus_mutex;
static int lcd_mutex_ready;
static int lcd_ready;
static int lcd_backlight_on;
static uint16_t lcd_controller_id;

/* 写命令：向 A18=0 的地址写 1 字节，FSMC 自动产生 8080 写时序。 */
static void lcd_write_command(uint8_t command)
{
    *LCD_COMMAND_ADDRESS = command;
}

/* 写数据：向 A18=1 的地址写 1 字节。 */
static void lcd_write_data(uint8_t data)
{
    *LCD_DATA_ADDRESS = data;
}

/* 读数据：从数据地址读 1 字节；读取需先丢弃空读字节（见 0x04 的用法）。 */
static uint8_t lcd_read_data(void)
{
    return *LCD_DATA_ADDRESS;
}

/* 发送"命令 + 参数序列"：先写命令字，再连续写参数字节。 */
static void lcd_write_bytes(uint8_t command, const uint8_t *data, size_t count)
{
    lcd_write_command(command);
    while (count-- > 0U)
    {
        lcd_write_data(*data++);
    }
}

/**
 * HAL callback which assigns every LCD bus signal to AF12/FSMC.
 * Data: PD14, PD15, PD0, PD1, PE7..PE10; control: PD4, PD5, PD13, PG10.
 *
 * 8080 并口接线（全部复用 AF12/FSMC）：
 *   数据 D0..D7 = PD14, PD15, PD0, PD1, PE7, PE8, PE9, PE10
 *   RD(NOE) = PD4，WR(NWE) = PD5，CS(NE3) = PD13，D/C(A18) = PG10
 * 复位 PD3、背光 PF9 为普通 GPIO，见 lcd_control_gpio_init()。
 */
void HAL_SRAM_MspInit(SRAM_HandleTypeDef *hsram)
{
    GPIO_InitTypeDef gpio = {0};
    RT_UNUSED(hsram);

    __HAL_RCC_FSMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
               GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &gpio);

    gpio.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOG, &gpio);
}

/* 配置复位/背光两个普通 GPIO，初始保持复位有效、背光关闭。 */
static void lcd_control_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Keep the panel dark and in reset while the external bus is configured. */
    HAL_GPIO_WritePin(LCD_RESET_GPIO, LCD_RESET_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_BL_GPIO, LCD_BL_PIN, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = LCD_RESET_PIN;
    HAL_GPIO_Init(LCD_RESET_GPIO, &gpio);
    gpio.Pin = LCD_BL_PIN;
    HAL_GPIO_Init(LCD_BL_GPIO, &gpio);
}

/* 用 NOR/SRAM 模式把 FSMC 配置成 8080 并口；ExtendedMode 使读、写可用两套时序。 */
static int lcd_fsmc_init(void)
{
    FMC_NORSRAM_TimingTypeDef read_timing = {0};
    FMC_NORSRAM_TimingTypeDef write_timing = {0};

    lcd_sram.Instance = FSMC_NORSRAM_DEVICE;
    lcd_sram.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
    lcd_sram.Init.NSBank = FSMC_NORSRAM_BANK3;
    lcd_sram.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    lcd_sram.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    lcd_sram.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;
    lcd_sram.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    lcd_sram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    lcd_sram.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    lcd_sram.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    lcd_sram.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    lcd_sram.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    lcd_sram.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    lcd_sram.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    lcd_sram.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
    lcd_sram.Init.PageSize = FSMC_PAGE_SIZE_NONE;

    /* 读时序偏保守：保证读 ID / 调试读取可靠。 */
    read_timing.AddressSetupTime = 15;
    read_timing.AddressHoldTime = 0;
    read_timing.DataSetupTime = 60;
    read_timing.BusTurnAroundDuration = 0;
    read_timing.CLKDivision = 0;
    read_timing.DataLatency = 0;
    read_timing.AccessMode = FSMC_ACCESS_MODE_A;

    /* 写时序更紧以加快刷屏；168 MHz 下与官方 RT-Spark 板级 BSP 相同。 */
    write_timing.AddressSetupTime = 9;
    write_timing.AddressHoldTime = 0;
    write_timing.DataSetupTime = 8;
    write_timing.BusTurnAroundDuration = 0;
    write_timing.CLKDivision = 0;
    write_timing.DataLatency = 0;
    write_timing.AccessMode = FSMC_ACCESS_MODE_A;

    return HAL_SRAM_Init(&lcd_sram, &read_timing, &write_timing) == HAL_OK ? 0 : -1;
}

/**
 * @brief 初始化复位/背光 GPIO、FSMC 总线和 ST7789 控制器。
 * @retval 0 成功；-1 互斥量创建或 FSMC 初始化失败。
 * @note 由 gui_thread 在 LVGL 初始化前调用一次；init 命令序列取自
 *       显示厂商参考代码，与 RT-Thread 官方 RT-Spark BSP 保持一致。
 */
int st7789_init(void)
{
    static const uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    static const uint8_t power[] = {0xA4, 0xA1};
    static const uint8_t gamma_positive[] = {
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    static const uint8_t gamma_negative[] = {
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
    const uint8_t zero = 0x00;

    if (rt_mutex_init(&lcd_bus_mutex, "lcd", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        return -1;
    }
    lcd_mutex_ready = 1;

    lcd_control_gpio_init();
    if (lcd_fsmc_init() != 0)
    {
        return -1;
    }
    /* 硬件复位：低电平保持 100 ms 后释放，再等待控制器完成上电初始化。 */
    HAL_GPIO_WritePin(LCD_RESET_GPIO, LCD_RESET_PIN, GPIO_PIN_RESET);
    rt_thread_mdelay(100);
    HAL_GPIO_WritePin(LCD_RESET_GPIO, LCD_RESET_PIN, GPIO_PIN_SET);
    rt_thread_mdelay(100);

    /* 与 RT-Thread 官方 RT-Spark BSP 一致：读取 0x04，正常控制器返回 0x81B3。 */
    /* 0x04 RDDID：前两次为空读须丢弃，随后两个字节拼成 16 位 ID。 */
    lcd_write_command(0x04);
    (void)lcd_read_data();
    (void)lcd_read_data();
    lcd_controller_id = (uint16_t)lcd_read_data() << 8;
    lcd_controller_id |= lcd_read_data();
    rt_kprintf("[LCD] ST7789 controller ID: 0x%04X\n", lcd_controller_id);

    /* 控制器初始化序列取自显示厂商参考代码；寄存器含义按 ST7789 手册标注。 */
    lcd_write_bytes(0x36, &zero, 1);       /* 0x36 MADCTL=0x00：正常扫描方向，不做行列交换/镜像。 */
    { const uint8_t format = 0x65; lcd_write_bytes(0x3A, &format, 1); } /* 0x3A COLMOD：低 3 位 5 = 16bit/像素 RGB565。 */
    lcd_write_bytes(0xB2, porch, sizeof(porch)); /* 0xB2 PORCTRL：前后消隐设置。 */
    { const uint8_t v = 0x35; lcd_write_bytes(0xB7, &v, 1); } /* 0xB7 GCTRL：门极驱动电压 VGH/VGL。 */
    { const uint8_t v = 0x37; lcd_write_bytes(0xBB, &v, 1); } /* 0xBB VCOMS：VCOM 电压。 */
    { const uint8_t v = 0x2C; lcd_write_bytes(0xC0, &v, 1); } /* 0xC0 LCMCTRL：LCM 控制。 */
    { const uint8_t v = 0x01; lcd_write_bytes(0xC2, &v, 1); } /* 0xC2 VDVVRHEN：使能 VDV/VRH 命令。 */
    { const uint8_t v = 0x12; lcd_write_bytes(0xC3, &v, 1); } /* 0xC3 VRHS：VRH 电压。 */
    { const uint8_t v = 0x20; lcd_write_bytes(0xC4, &v, 1); } /* 0xC4 VDVS：VDV 电压。 */
    { const uint8_t v = 0x0F; lcd_write_bytes(0xC6, &v, 1); } /* 0xC6 FRCTRL2：普通模式帧率控制。 */
    lcd_write_bytes(0xD0, power, sizeof(power)); /* 0xD0 PWCTRL1：电源控制。 */
    lcd_write_bytes(0xE0, gamma_positive, sizeof(gamma_positive)); /* 0xE0 PVGAMCTRL：正极性伽马校正。 */
    lcd_write_bytes(0xE1, gamma_negative, sizeof(gamma_negative)); /* 0xE1 NVGAMCTRL：负极性伽马校正。 */
    lcd_write_command(0x21);               /* 0x21 INVON：开反色显示，该 IPS 面板需要。 */
    lcd_write_bytes(0x35, &zero, 1);       /* 0x35 TEON=0x00：开启 tearing effect 输出（仅 V-blanking）。 */
    lcd_write_command(0x11);               /* 0x11 SLPOUT：退出睡眠。 */
    rt_thread_mdelay(120);                 /* SLPOUT 之后需等待 120 ms 才能开显示。 */
    lcd_write_command(0x29);               /* 0x29 DISPON：开显示。 */

    lcd_ready = 1;

    /* 原理图与官方 BSP 均为 PF9 高电平驱动 NPN，PF8 是红外接收输入。 */
    st7789_set_backlight(1);
    rt_kprintf("[LCD] Backlight PF9: ON\n");
    return 0;
}

/**
 * @brief 设置 GRAM 写入窗口并切换到像素接收状态。
 * @param x1/x2 起始/结束列，y1/y2 起始/结束行（均含端点）。
 * @note 0x2A/0x2B 分别设置列、行范围（16 位坐标、高字节在前），随后的
 *       0x2C(RAMWR) 开始接收像素流；GRAM 地址按行优先自动递增，
 *       写满窗口后回卷到窗口起点。MADCTL=0x00 约定下 X=列、Y=行，
 *       坐标原点在面板左上角。
 */
void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    const uint8_t columns[] = {(uint8_t)(x1 >> 8), (uint8_t)x1,
                               (uint8_t)(x2 >> 8), (uint8_t)x2};
    const uint8_t rows[] = {(uint8_t)(y1 >> 8), (uint8_t)y1,
                            (uint8_t)(y2 >> 8), (uint8_t)y2};

    lcd_write_bytes(0x2A, columns, sizeof(columns));
    lcd_write_bytes(0x2B, rows, sizeof(rows));
    lcd_write_command(0x2C);
}

/**
 * @brief 向当前窗口顺序写入 RGB565 像素。
 * @param pixels RGB565 像素数组；count 像素个数（须与窗口面积一致）。
 */
void st7789_write_pixels(const uint16_t *pixels, size_t count)
{
    while (count-- > 0U)
    {
        const uint16_t color = *pixels++;
        /* 总线为 8 位，每个 RGB565 像素先送高字节（ST7789 要求 MSB first）。 */
        lcd_write_data((uint8_t)(color >> 8));
        lcd_write_data((uint8_t)color);
    }
}

/**
 * @brief 加互斥锁地刷新一个矩形区域。
 * LVGL 刷新线程与 MSH 诊断命令（lcd_test 等）会并发访问总线，
 * 不加锁会让窗口命令与像素数据交叉而花屏；lcd_mutex_ready 未就绪时
 * 退化为不加锁直写。
 */
void st7789_blit(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                 const uint16_t *pixels, size_t count)
{
    if (lcd_mutex_ready != 0)
    {
        (void)rt_mutex_take(&lcd_bus_mutex, RT_WAITING_FOREVER);
    }
    st7789_set_window(x1, y1, x2, y2);
    st7789_write_pixels(pixels, count);
    if (lcd_mutex_ready != 0)
    {
        (void)rt_mutex_release(&lcd_bus_mutex);
    }
}

/**
 * @brief 整屏填充单一颜色（MSH lcd_test 诊断用）。
 * 初始化未完成时直接返回，避免在 FSMC 未配置好时访问总线。
 */
void st7789_fill_color(uint16_t color)
{
    size_t count = (size_t)ST7789_WIDTH * ST7789_HEIGHT;

    if (lcd_ready == 0)
    {
        return;
    }
    (void)rt_mutex_take(&lcd_bus_mutex, RT_WAITING_FOREVER);
    st7789_set_window(0, 0, ST7789_WIDTH - 1U, ST7789_HEIGHT - 1U);
    while (count-- > 0U)
    {
        lcd_write_data((uint8_t)(color >> 8));
        lcd_write_data((uint8_t)color);
    }
    (void)rt_mutex_release(&lcd_bus_mutex);
}

/* 设置背光：PF9 高电平经 NPN 点亮背光，低电平熄灭。 */
void st7789_set_backlight(int on)
{
    lcd_backlight_on = (on != 0);
    HAL_GPIO_WritePin(LCD_BL_GPIO, LCD_BL_PIN,
                      lcd_backlight_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 读取当前背光状态。 */
int st7789_get_backlight(void)
{
    return lcd_backlight_on;
}

/* 查询 LCD 是否已完成初始化。 */
int st7789_is_ready(void)
{
    return lcd_ready;
}

/* 返回初始化时读到的控制器 ID。 */
uint16_t st7789_get_id(void)
{
    return lcd_controller_id;
}
