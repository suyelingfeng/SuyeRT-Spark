/**
 * @file usart.c
 * @brief USART1（ST-LINK 虚拟串口，115200-8-N-1）的初始化与 MSP 回调。
 *
 * 数据流：rt_kprintf/FinSH -> rt_hw_console_output()/rt_hw_console_getchar()
 * （BSP/rt_hw_console.c）-> huart1 -> USART1(PA9/PA10)。
 * 接收中断 USART1_IRQHandler 也在 rt_hw_console.c 中实现，不在本文件。
 */
#include "usart.h"

/* USART1 全局句柄；控制台收发（BSP/rt_hw_console.c）直接操作它。 */
UART_HandleTypeDef huart1;

/**
 * @brief 初始化 USART1：115200-8-N-1，无硬件流控，收发模式。
 * @note  USART1 挂在 APB2（84 MHz）上；对应 ST-LINK 虚拟串口，
 *        是 rt_kprintf/FinSH 控制台的物理通道。失败时进入 Error_Handler()。
 * @retval 无
 */
void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* HAL_UART_Init() 内部回调：使能 USART1/GPIOA 时钟，
 * 并把 PA9/PA10 复用为 USART1_TX/RX（AF7）；上拉保证空闲时 TX 线保持高电平。 */
void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* 反初始化：关闭 USART1 时钟并释放 PA9/PA10，本工程正常运行时不会走到。 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{
    if (uartHandle->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
}
