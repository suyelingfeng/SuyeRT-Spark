/** @file rw007_hw.h  RW007 模块的板级复位和状态管脚驱动。 */
#ifndef RW007_HW_H
#define RW007_HW_H

#include <stdbool.h>

/** RW007 启动状态快照，由本模块的 rw007_hw_* 函数更新。 */
typedef struct
{
    bool reset_released; /**< 复位管脚是否已释放（模块开始启动） */
    bool ready;          /**< 模块是否已就绪（INT/BUSY 变高） */
    bool int_high;       /**< 最近一次读到的 INT/BUSY 电平 */
} rw007_hw_status_t;

/**
 * @brief 查询 INT/BUSY 管脚电平；不会释放模块复位。
 * @param status 状态快照，int_high 被刷新；仅在已释放复位且 INT 为高时置位 ready。
 */
void rw007_hw_read_status(rw007_hw_status_t *status);

/**
 * @brief 按 RT-Thread 官方端口时序复位并启动模块。
 * @param status 状态快照，返回时 ready 反映模块是否按时就绪。
 */
void rw007_hw_reset_and_start(rw007_hw_status_t *status);

#endif
