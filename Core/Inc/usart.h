/**
 * @file usart.h
 * @brief USART1 句柄与初始化接口；使用方为 main.c 和 BSP/rt_hw_console.c。
 */
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* USART1 全局句柄，定义在 usart.c；控制台收发（rt_hw_console.c）直接使用它。 */
extern UART_HandleTypeDef huart1;

/**
 * @brief 初始化 USART1（115200-8-N-1），由 main() 在启动 RTOS 前调用。
 * @retval 无
 */
void MX_USART1_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif
