/**
 * @file board_storage.c
 * @brief 板载 W25Q 的 JEDEC ID 读取及 SD 卡检测逻辑。
 *
 * 数据流：spi2_board 提供裸字节传输 -> 本文件发 0x9F 命令读出器件身份 ->
 * 上层（启动信息/板级服务）据此判断存储器件是否可用。
 */
#include "board_storage.h"
#include "spi2_board.h"
#include "main.h"

/**
 * @brief 探测 W25Q JEDEC ID 与 SD 卡在位状态。
 * @param status 输出参数；flash_ok 与 flash_size_kib 由 JEDEC 容量字节换算。
 */
void board_storage_probe(board_storage_status_t *status)
{
    if (status == NULL) return;
    /* 访问 Flash 前确保 RW007 片选保持无效，避免共享总线冲突。 */
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    /* 0x9F = Read JEDEC ID；随后发 3 个空字节（0xFF）把 ID 的三个字节移出来。 */
    (void)spi2_board_transfer(0x9FU);
    status->flash_jedec[0] = spi2_board_transfer(0xFFU);
    status->flash_jedec[1] = spi2_board_transfer(0xFFU);
    status->flash_jedec[2] = spi2_board_transfer(0xFFU);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

    /* 全 0 或全 1 说明总线上没有器件应答（悬空或短路）；容量字节合法范围为 0x10~0x1F。 */
    status->flash_ok = (status->flash_jedec[0] != 0x00U) &&
                       (status->flash_jedec[0] != 0xFFU) &&
                       (status->flash_jedec[2] >= 0x10U) &&
                       (status->flash_jedec[2] <= 0x1FU);
    /* 容量字节是字节数的 log2，如 W25Q16 = 0x15 -> 2^21 字节 = 2 MiB。 */
    status->flash_size_kib = status->flash_ok
        ? (1UL << (status->flash_jedec[2] - 10U)) : 0U;
    /* PF3 为 SD 卡检测脚，插卡时接地呈低电平。 */
    status->sd_inserted =
        HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_3) == GPIO_PIN_RESET;
}
