/** @file gpio_monitor.c  Read-only STM32F407 GPIO register monitor. */
#include "gpio_monitor.h"
#include "main.h"

static GPIO_TypeDef *const gpio_ports[GPIO_MONITOR_PORT_COUNT] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH, GPIOI
};

void gpio_monitor_init(void)
{
    /* Clock enabling does not change any pin mode or level. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
}

void gpio_monitor_sample(gpio_monitor_port_t ports[GPIO_MONITOR_PORT_COUNT])
{
    if (ports == 0) return;
    for (uint8_t i = 0U; i < GPIO_MONITOR_PORT_COUNT; ++i)
    {
        ports[i].input = (uint16_t)gpio_ports[i]->IDR;
        ports[i].output = (uint16_t)gpio_ports[i]->ODR;
        ports[i].mode = gpio_ports[i]->MODER;
    }
}
