/**
 * @file board_storage.c
 * @brief 板载 W25Q Flash 和 SD 卡检测逻辑（与通用 SPI 驱动集成）
 *
 * 本模块提供存储设备的检测和识别功能。
 * W25Q Flash 的 JEDEC ID 读取由 w25q_device 驱动层处理，
 * 本模块仅负责汇总检测结果。
 *
 * 数据流：
 * 1. spi_driver_init() 初始化通用 SPI 框架
 * 2. spi2_adapter_register() 注册 SPI2 适配层
 * 3. spi_device_open() 打开 W25Q 设备
 * 4. w25q_device_init() 初始化 W25Q 驱动
 * 5. w25q_device_read_jedec_id() 读取 JEDEC ID
 * 6. board_storage_probe() 汇总检测结果
 */

#include "board_storage.h"
#include "w25q_device.h"
#include "main.h"
#include <rtthread.h>

/**
 * @brief 探测 W25Q JEDEC ID 与 SD 卡在位状态
 *
 * @param[out] status 输出参数
 *        - flash_jedec：W25Q JEDEC ID（3 字节）
 *        - flash_ok：Flash 是否可用
 *        - flash_size_kib：Flash 容量（KiB）
 *        - sd_inserted：SD 卡是否插入
 *
 * @note 假设已完成 spi_driver_init()、spi2_adapter_register() 和 w25q_device_init()。
 */
void board_storage_probe(board_storage_status_t *status)
{
    if (status == NULL)
        return;
    
    /* 初始化为默认值 */
    status->flash_jedec[0] = 0xFF;
    status->flash_jedec[1] = 0xFF;
    status->flash_jedec[2] = 0xFF;
    status->flash_ok = false;
    status->flash_size_kib = 0;
    
    /* 尝试读取 W25Q JEDEC ID */
    w25q_jedec_id_t jedec_id = {0};
    if (w25q_device_read_jedec_id(&jedec_id) == 0)
    {
        status->flash_jedec[0] = jedec_id.manufacturer;
        status->flash_jedec[1] = jedec_id.device_id_high;
        status->flash_jedec[2] = jedec_id.device_id_low;
        
        /* 全 0 或全 1 说明总线上没有器件应答（悬空或短路）；
           容量字节合法范围为 0x10~0x1F */
        status->flash_ok = (status->flash_jedec[0] != 0x00U) &&
                          (status->flash_jedec[0] != 0xFFU) &&
                          (status->flash_jedec[2] >= 0x10U) &&
                          (status->flash_jedec[2] <= 0x1FU);
        
        /* 容量字节是字节数的 log2，如 W25Q16 = 0x15 -> 2^21 字节 = 2 MiB */
        if (status->flash_ok)
        {
            status->flash_size_kib = (1UL << (status->flash_jedec[2] - 10U));
            rt_kprintf("[STORAGE] W25Q detected: %u KiB\n", status->flash_size_kib);
        }
        else
        {
            rt_kprintf("[STORAGE] W25Q JEDEC ID invalid or device not responding\n");
        }
    }
    else
    {
        rt_kprintf("[STORAGE] Failed to read W25Q JEDEC ID\n");
    }
    
    /* PF3 为 SD 卡检测脚，插卡时接地呈低电平 */
    status->sd_inserted = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_3) == GPIO_PIN_RESET;
    
    if (status->sd_inserted)
    {
        rt_kprintf("[STORAGE] SD card detected\n");
    }
}
