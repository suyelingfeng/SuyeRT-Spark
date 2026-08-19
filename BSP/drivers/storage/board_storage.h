/**
 * @file board_storage.h
 * @brief W25Q Flash 探测和 SD 插卡检测。
 *
 * 本模块只在探测阶段读取器件身份信息，真正的数据读写由上层存储服务完成。
 */
#ifndef BOARD_STORAGE_H
#define BOARD_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

/** 板载存储器件的探测结果。 */
typedef struct
{
    bool flash_ok;           /**< W25Q 是否给出有效应答 */
    uint8_t flash_jedec[3];  /**< JEDEC ID：厂商 ID、存储类型、容量编码 */
    uint32_t flash_size_kib; /**< 由容量编码换算的 Flash 容量（KiB），无效时为 0 */
    bool sd_inserted;        /**< SD 卡是否插入（检测脚低有效） */
} board_storage_status_t;

/**
 * @brief 探测 W25Q 的 JEDEC ID 并读取 SD 卡检测脚电平。
 * @param status 输出参数，探测结果写入该结构体。
 */
void board_storage_probe(board_storage_status_t *status);

#endif
