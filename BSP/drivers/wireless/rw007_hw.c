/**
 * @file rw007_hw.c
 * @brief RW007 复位、启动等待和 INT/BUSY 读取。
 *
 * 数据流：spi2_board_init 配好管脚并保持模块复位 -> 本文件按需释放复位、
 * 轮询等待模块就绪 -> RT-Thread rw007 软件包接管 SPI2 上的 Wi-Fi 通信。
 */
#include "rw007_hw.h"
#include "main.h"
#include <rtthread.h>

/**
 * @brief 读取 INT/BUSY 管脚电平并刷新状态快照。
 * @param status 状态快照；只更新 int_high，并在条件满足时置位 ready。
 */
void rw007_hw_read_status(rw007_hw_status_t *status)
{
    if (status == RT_NULL) return;
    status->int_high = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_11) == GPIO_PIN_SET;
    /* ready 只置位不清零：模块一旦就绪即视为启动完成，之后 INT 电平归 rw007 驱动解释。 */
    if (status->reset_released && status->int_high) status->ready = true;
}

/**
 * @brief 按官方时序复位并启动 RW007。
 * @param status 状态快照，返回时 ready 表示模块是否按时就绪。
 *
 * 时序依据 RT-Thread rw007 软件包对移植层的要求：
 * 1. 复位期间片选置高，避免误触发 SPI 事务；RST 拉低保持 100 ms 让模块彻底复位；
 * 2. 释放 RST 后，模块启动期间会拉低 INT/BUSY，最多轮询 500 ms 等其变高；
 * 3. 就绪后再等 200 ms，给模块固件留出内部初始化时间；
 * 4. 启动阶段 PG11 由 spi2_board_init 配为下拉输入以维持确定电平；
 *    轮询结束后改为上拉输入，交由 rw007 驱动作为 INT/BUSY 信号使用。
 */
void rw007_hw_reset_and_start(rw007_hw_status_t *status)
{
    GPIO_InitTypeDef gpio = {0};
    if (status == RT_NULL) return;
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);   /* CS 置高：复位期间不参与总线 */
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_15, GPIO_PIN_RESET); /* 拉低 RST 开始复位 */
    status->reset_released = false;
    status->ready = false;
    rt_thread_mdelay(100U);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_15, GPIO_PIN_SET);   /* 释放复位，模块开始启动 */
    status->reset_released = true;

    /* 100 次 * 5 ms = 最多等 500 ms；超时则 ready 保持 false，由上层决定是否重试。 */
    for (uint16_t wait = 0U; wait < 100U; ++wait)
    {
        if (HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_11) == GPIO_PIN_SET)
        {
            status->ready = true;
            break;
        }
        rt_thread_mdelay(5U);
    }
    rt_thread_mdelay(200U);

    /* 启动流程结束，把 INT/BUSY 从下拉改为上拉输入，匹配 rw007 驱动的信号约定。 */
    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &gpio);
    rw007_hw_read_status(status);
}
