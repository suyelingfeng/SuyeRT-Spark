/**
 * @file spi2_board.c
 * @brief 只负责 SPI2 电气配置和字节传输，不解释任何器件命令。
 *
 * 本总线不走 HAL SPI 句柄，直接按位配置 SPI2 寄存器；
 * 片选采用软件管理（GPIO 拉低有效），多器件共享时由调用方保证
 * 同一时刻只拉低一个片选。
 */
#include "spi2_board.h"
#include "main.h"

/**
 * @brief 初始化 SPI2 外设、板载器件片选及 RW007/SD 相关管脚。
 *
 * 完成后 SPI2 处于主机模式待机；RW007 保持复位态，由 rw007_hw 按联网需求释放。
 */
void spi2_board_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    /* SCK/MISO/MOSI 复用为 AF5，高速摆率以保证 10 MHz 级时钟的边沿质量。 */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI2;
    gpio.Pin = GPIO_PIN_13;             /* PB13: SCK */
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3; /* PC2: MISO, PC3: MOSI */
    HAL_GPIO_Init(GPIOC, &gpio);

    /* 片选与复位用普通推挽输出；上拉保证初始化瞬间不误导通。 */
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Alternate = 0U;
    gpio.Pin = GPIO_PIN_12;             /* PB12: W25Q CS */
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_10;             /* PF10: RW007 CS */
    HAL_GPIO_Init(GPIOF, &gpio);
    gpio.Pin = GPIO_PIN_15;             /* PG15: RW007 RST */
    HAL_GPIO_Init(GPIOG, &gpio);
    /* 两片选先置高（无效），避免上电瞬间误选中器件。 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);
    /* 保持 RW007 复位，只有用户明确请求联网时才释放。 */
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_15, GPIO_PIN_RESET);

    /* 启动阶段 INT/BUSY 配下拉输入维持确定电平；就绪后由 rw007_hw 改为上拉。 */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Pin = GPIO_PIN_11;             /* PG11: RW007 INT/BUSY */
    HAL_GPIO_Init(GPIOG, &gpio);
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = GPIO_PIN_3;              /* PF3: SD detect，低有效 */
    HAL_GPIO_Init(GPIOF, &gpio);

    /*
     * CR1 各位含义：
     *   MSTR    主机模式；
     *   SSM+SSI 软件 NSS 管理（NSS 内部拉高），片选完全交给上面的 GPIO；
     *   BR_1    波特率分频 0b010 = fPCLK/4，SPI2 挂在 42 MHz APB1 上，即 10.5 MHz；
     *   DFF/CPOL/CPHA 未置位 = 8 位数据帧、SPI 模式 0。
     */
    SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1;
    SPI2->CR2 = 0U;
    SPI2->CR1 |= SPI_CR1_SPE; /* 最后使能 SPI，避免配置过程中发出毛刺时钟。 */
}

/**
 * @brief 全双工传输一个字节。
 * @param value 要发送的字节。
 * @retval 同一时钟周期内从 MISO 读回的字节。
 */
uint8_t spi2_board_transfer(uint8_t value)
{
    /* 超时计数防止器件异常（如无应答）时永久死等。 */
    uint32_t timeout = 100000U;
    while (((SPI2->SR & SPI_SR_TXE) == 0U) && (--timeout != 0U)) { }
    /* 强制按 8 位访问数据寄存器，确保一次只收发一个 8 位帧。 */
    *(__IO uint8_t *)&SPI2->DR = value;
    timeout = 100000U;
    while (((SPI2->SR & SPI_SR_RXNE) == 0U) && (--timeout != 0U)) { }
    return *(__IO uint8_t *)&SPI2->DR;
}
