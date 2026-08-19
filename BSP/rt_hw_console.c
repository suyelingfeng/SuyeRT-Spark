/**
 * @file rt_hw_console.c
 * @brief RT-Thread 控制台到 USART1 的硬件对接层。
 *
 * 数据流：rt_kprintf()/FinSH 输出 -> rt_hw_console_output() -> USART1 逐字节发送；
 * USART1 接收中断 -> 环形缓冲 -> rt_hw_console_getchar() -> FinSH 线程解析命令。
 * 串口参数 115200-8-N-1，对应 ST-LINK 虚拟串口，初始化见 Core/Src/usart.c。
 */
#include "board.h"
#include "usart.h"

/* 环形缓冲容量：128 字节足以容纳一整行粘贴进来的 MSH 命令。 */
#define CONSOLE_RX_BUFFER_SIZE 128U

/* USART1 接收中断写入 head，FinSH 线程只读取 tail，单生产者/单消费者无需加锁。 */
static volatile uint16_t console_rx_head;
static volatile uint16_t console_rx_tail;
static uint8_t console_rx_buffer[CONSOLE_RX_BUFFER_SIZE];
static rt_bool_t console_rx_started;

/* 惰性开启接收中断：仅在 FinSH 首次取字符时使能一次，启动早期的 rt_kprintf 输出不依赖它。 */
static void console_rx_start(void)
{
    if (console_rx_started == RT_FALSE)
    {
        console_rx_started = RT_TRUE;
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
        HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

/**
 * @brief USART1 中断接收入口。
 *
 * HAL 的轮询接收会在电脑一次发送整行命令时丢字节；环形缓冲先完整接收，
 * FinSH 再按自己的速度逐字读取，因此手工输入和粘贴命令都可靠。
 */
void USART1_IRQHandler(void)
{
    /* 先缓存 SR：后面读 DR 会同时清掉 RXNE/错误标志，必须先留存入口时的状态。 */
    uint32_t status = huart1.Instance->SR;

    rt_interrupt_enter();
    if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U)
    {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFFU); /* 读取 DR 同时清除 RXNE/错误标志。 */

        if ((status & USART_SR_RXNE) != 0U)
        {
            uint16_t next = (uint16_t)((console_rx_head + 1U) % CONSOLE_RX_BUFFER_SIZE);
            /* 缓冲区满时丢弃新字节而不是覆盖旧数据：丢尾部新字符比弄乱整行命令更安全。 */
            if (next != console_rx_tail)
            {
                console_rx_buffer[console_rx_head] = ch;
                console_rx_head = next;
            }
        }
    }
    rt_interrupt_leave();
}

/**
 * @brief 控制台输出口，RT-Thread 内核的 rt_kprintf() 最终调用本函数。
 * @param str 以 '\0' 结尾的字符串，允许为 RT_NULL（直接返回）。
 *
 * 采用 HAL 轮询方式逐字节发送，不依赖中断与调度器，因此 rt_show_version()
 * 等调度器启动前的早期日志也能正常输出。遇到 '\n' 先补发 '\r'：
 * 串口终端按 CRLF 换行，只发 LF 会导致显示错位。
 */
void rt_hw_console_output(const char *str)
{
    if (str == RT_NULL)
    {
        return;
    }

    while (*str != '\0')
    {
        if (*str == '\n')
        {
            const uint8_t cr = '\r';
            HAL_UART_Transmit(&huart1, (uint8_t *)&cr, 1, HAL_MAX_DELAY);
        }

        HAL_UART_Transmit(&huart1, (uint8_t *)str, 1, HAL_MAX_DELAY);
        str++;
    }
}

/**
 * @brief 控制台读字符口，FinSH 线程通过本函数逐个取回用户输入。
 * @retval 读取到的字符（按 signed char 返回，与 FinSH 的接口约定一致）。
 *
 * 缓冲区空时用 rt_thread_mdelay(1) 轮询而非忙等：FinSH 优先级低，
 * 忙等会饿死同优先级及更低优先级线程，延时让出 CPU 后功耗也更低。
 */
signed char rt_hw_console_getchar(void)
{
    uint8_t ch;

    console_rx_start();
    while (console_rx_head == console_rx_tail)
    {
        rt_thread_mdelay(1);
    }

    ch = console_rx_buffer[console_rx_tail];
    console_rx_tail = (uint16_t)((console_rx_tail + 1U) % CONSOLE_RX_BUFFER_SIZE);
    return (signed char)ch;
}
