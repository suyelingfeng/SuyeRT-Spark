/**
 * @file spi2_board.h
 * @brief RT-Spark 板载 SPI2 总线及共享管脚。
 *
 * SPI2 上挂 W25Q Flash 和 RW007 Wi-Fi 模块，片选各自独立；
 * 本头文件只暴露总线初始化与单字节全双工传输，器件协议由各驱动自行实现。
 */
#ifndef SPI2_BOARD_H
#define SPI2_BOARD_H

#include <stdint.h>

/**
 * @brief 初始化 SPI2、Flash/RW007 片选、RW007 复位/中断和 SD 检测管脚。
 * @note 初始化后 RW007 保持复位态，由 rw007_hw 按联网需求释放。
 */
void spi2_board_init(void);

/**
 * @brief 发送一个字节并返回同时收到的字节。
 * @param value 要发送的字节。
 * @retval 同一时钟周期内从 MISO 读回的字节。
 */
uint8_t spi2_board_transfer(uint8_t value);

#endif
