/**
 * @file soft_i2c.c
 * @brief 可复用的软件 I2C 位操作实现；不包含任何具体传感器寄存器。
 *
 * 数据流：上层传感器驱动（AHT21/AP3216C/ICM20608）-> 本文件的
 * soft_i2c_* 事务接口 -> start/stop/write_byte/read_byte 位操作 ->
 * delay_us 微秒延时 + GPIO 开漏读写。
 */
#include "soft_i2c.h"

/* 半周期 4 µs，整位约 8 µs，即总线速率约 125 kHz；需要严格 100 kHz 时可调大该值。 */
#define I2C_HALF_PERIOD_US 4U

/*
 * 用 DWT 周期计数器实现 µs 级忙等：
 * HAL_Delay 只有 1 ms 粒度，无法满足 I2C 位时序；
 * (uint32_t)(CYCCNT - start) 的无符号减法在计数器回绕时结果仍然正确。
 */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000U) * us;
    while ((uint32_t)(DWT->CYCCNT - start) < ticks) { }
}

/**
 * @brief 使能 DWT 的 CYCCNT 计数器，作为 delay_us 的时基。
 * @note 全局只需调用一次，且必须先于任何总线事务。
 */
void soft_i2c_timebase_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* 按端口使能 GPIO 时钟；目前板上传感器只挂在 E/F 口，新增端口时在此补分支。 */
static void enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
    else if (port == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
}

/**
 * @brief 把 SCL/SDA 配为开漏输出并释放总线。
 * @param bus 总线描述。
 *
 * 开漏 + 上拉是 I2C 的电气基础：主机只能把线拉低，
 * 写"高"实际是释放总线靠上拉电阻维持高电平，避免多机推挽冲突。
 */
void soft_i2c_bus_init(const soft_i2c_bus_t *bus)
{
    GPIO_InitTypeDef gpio = {0};
    enable_gpio_clock(bus->port);
    gpio.Pin = bus->scl_pin | bus->sda_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->port, &gpio);
    HAL_GPIO_WritePin(bus->port, gpio.Pin, GPIO_PIN_SET);
}

/* 单线电平写入的薄封装，让位时序代码按 SCL/SDA 语义阅读。 */
static void scl(const soft_i2c_bus_t *bus, GPIO_PinState state)
{
    HAL_GPIO_WritePin(bus->port, bus->scl_pin, state);
}

static void sda(const soft_i2c_bus_t *bus, GPIO_PinState state)
{
    HAL_GPIO_WritePin(bus->port, bus->sda_pin, state);
}

/* START 条件：SCL 高电平期间 SDA 产生下降沿。 */
static void start(const soft_i2c_bus_t *bus)
{
    sda(bus, GPIO_PIN_SET); scl(bus, GPIO_PIN_SET); delay_us(I2C_HALF_PERIOD_US);
    sda(bus, GPIO_PIN_RESET); delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_RESET);
}

/* STOP 条件：SCL 高电平期间 SDA 产生上升沿。 */
static void stop(const soft_i2c_bus_t *bus)
{
    sda(bus, GPIO_PIN_RESET); delay_us(I2C_HALF_PERIOD_US);
    scl(bus, GPIO_PIN_SET); delay_us(I2C_HALF_PERIOD_US);
    sda(bus, GPIO_PIN_SET); delay_us(I2C_HALF_PERIOD_US);
}

/*
 * 发送一个字节（MSB 先行），随后在第 9 个时钟读 ACK：
 * 主机释放 SDA，从机把线拉低即表示应答。
 */
static bool write_byte(const soft_i2c_bus_t *bus, uint8_t value)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        sda(bus, (value & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_SET);
        delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_RESET); value <<= 1U;
    }
    sda(bus, GPIO_PIN_SET); delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_SET);
    delay_us(I2C_HALF_PERIOD_US);
    bool ack = HAL_GPIO_ReadPin(bus->port, bus->sda_pin) == GPIO_PIN_RESET;
    scl(bus, GPIO_PIN_RESET);
    return ack;
}

/*
 * 接收一个字节：SCL 高电平期间逐位采样 SDA。
 * ack=false 时主机回 NACK，告知从机这是本次传输的最后一个字节。
 */
static uint8_t read_byte(const soft_i2c_bus_t *bus, bool ack)
{
    uint8_t value = 0U;
    sda(bus, GPIO_PIN_SET);
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        value <<= 1U; delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_SET);
        if (HAL_GPIO_ReadPin(bus->port, bus->sda_pin) == GPIO_PIN_SET) value |= 1U;
        delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_RESET);
    }
    sda(bus, ack ? GPIO_PIN_RESET : GPIO_PIN_SET);
    delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_SET);
    delay_us(I2C_HALF_PERIOD_US); scl(bus, GPIO_PIN_RESET); sda(bus, GPIO_PIN_SET);
    return value;
}

/**
 * @brief 向从机写入一段数据。
 * @param bus     总线描述。
 * @param address 7 位从机地址（左移 1 位后读写位为 0）。
 * @param data    待发送数据缓冲区。
 * @param length  待发送字节数。
 * @retval true  全部字节收到 ACK；false 任一字节未应答，提前终止。
 */
bool soft_i2c_write(const soft_i2c_bus_t *bus, uint8_t address,
                    const uint8_t *data, uint8_t length)
{
    bool ok;
    start(bus); ok = write_byte(bus, (uint8_t)(address << 1U));
    for (uint8_t i = 0U; ok && i < length; ++i) ok = write_byte(bus, data[i]);
    stop(bus);
    return ok;
}

/**
 * @brief 从从机读取一段数据。
 * @param bus     总线描述。
 * @param address 7 位从机地址（读写位置 1）。
 * @param data    接收数据缓冲区。
 * @param length  要读取的字节数。
 * @retval true  地址帧收到 ACK；false 从机未应答地址。
 */
bool soft_i2c_read(const soft_i2c_bus_t *bus, uint8_t address,
                   uint8_t *data, uint8_t length)
{
    bool ok;
    start(bus); ok = write_byte(bus, (uint8_t)((address << 1U) | 1U));
    for (uint8_t i = 0U; ok && i < length; ++i) data[i] = read_byte(bus, i + 1U < length);
    stop(bus);
    return ok;
}

/**
 * @brief 读取从机寄存器。
 * @param bus     总线描述。
 * @param address 7 位从机地址。
 * @param reg     寄存器地址。
 * @param data    接收数据缓冲区。
 * @param length  要读取的字节数。
 * @retval true  全部阶段收到 ACK；false 任一阶段未应答。
 *
 * 先写寄存器地址，再以重复 START（中间无 STOP）切换为读方向，
 * 这是多数传感器寄存器读取协议要求的序列。
 */
bool soft_i2c_read_regs(const soft_i2c_bus_t *bus, uint8_t address,
                        uint8_t reg, uint8_t *data, uint8_t length)
{
    bool ok;
    start(bus); ok = write_byte(bus, (uint8_t)(address << 1U));
    if (ok) ok = write_byte(bus, reg);
    if (ok) { start(bus); ok = write_byte(bus, (uint8_t)((address << 1U) | 1U)); }
    for (uint8_t i = 0U; ok && i < length; ++i) data[i] = read_byte(bus, i + 1U < length);
    stop(bus);
    return ok;
}

/**
 * @brief 向从机寄存器写入一个字节。
 * @param bus     总线描述。
 * @param address 7 位从机地址。
 * @param reg     寄存器地址。
 * @param value   要写入的值。
 * @retval true  写入过程收到 ACK；false 任一字节未应答。
 */
bool soft_i2c_write_reg(const soft_i2c_bus_t *bus, uint8_t address,
                        uint8_t reg, uint8_t value)
{
    /* 寄存器写 = 一次写事务内依次发送 [reg, value]。 */
    uint8_t bytes[2] = {reg, value};
    return soft_i2c_write(bus, address, bytes, 2U);
}
