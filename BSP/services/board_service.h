/**
 * @file board_service.h
 * @brief RT-Spark 板载外设服务编排层。
 *
 * UI 不直接访问 GPIO/I2C/SPI，只读取本文件提供的快照。以后更换开发板、
 * 改用 RT-Thread 官方 rt_device 驱动时，只需替换服务层而不必重写 LVGL。
 */
#ifndef BOARD_SERVICE_H
#define BOARD_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#define BOARD_GPIO_PORT_COUNT 9U

typedef struct
{
    uint16_t input;
    uint16_t output;
    uint32_t mode;
} board_gpio_port_snapshot_t;

/**
 * 板级状态快照：UI/MSH 的只读视图。
 * 所有物理量以定点整数传输，字段后缀标明倍率（如 _x10 表示真实值 = 字段/10）。
 */
typedef struct
{
    uint32_t sequence;             /* 每轮处理 +1，UI 据此判断快照是否更新 */

    bool aht21_ok;
    int16_t temperature_x10;       /* 0.1 摄氏度 */
    uint16_t humidity_x10;         /* 0.1 %RH */
    int16_t temperature_kalman_x10;/* Kalman 后温度，0.1 摄氏度 */
    uint16_t humidity_kalman_x10;  /* Kalman 后湿度，0.1 %RH */
    bool environment_filter_ready;

    bool ap3216c_ok;
    uint32_t ambient_light_x10;    /* 0.1 lux */
    uint16_t proximity;

    bool icm20608_ok;
    int16_t accel_mg[3];           /* X/Y/Z，单位 mg */
    int16_t gyro_dps_x10[3];       /* 温度/零偏补偿后角速度，0.1 dps */
    int16_t imu_temperature_x10;   /* 芯片温度，0.1 摄氏度 */

    bool attitude_ready;           /* 完成开机静止校准和方向归零 */
    bool attitude_stationary;      /* 已连续检测到静止 */
    uint16_t attitude_calibration_samples; /* 已累积的开机静止样本数 */
    uint16_t attitude_calibration_target;  /* 校准所需样本数（300） */
    bool gyro_temperature_compensation_ready; /* 温漂系数已学到，可前馈补偿 */
    int16_t quaternion_x10000[4];  /* 相对零方向四元数 w/x/y/z */
    int16_t roll_x10;              /* Kalman 后横滚角，0.1 度 */
    int16_t pitch_x10;             /* Kalman 后俯仰角，0.1 度 */
    int16_t yaw_x10;               /* Kalman 后相对航向角，0.1 度 */
    int16_t gyro_bias_x1000[3];    /* 在线估计零偏，0.001 dps */
    int16_t gyro_temp_coeff_x10000[3]; /* 温漂系数，0.0001 dps/°C */

    bool flash_ok;
    uint8_t flash_jedec[3];        /* W25Q 系列 JEDEC ID */
    uint32_t flash_size_kib;
    bool sd_inserted;              /* SD 卡插拔状态 */

    bool rw007_reset_released;     /* RW007 复位已释放（模组已上电启动） */
    bool rw007_ready;              /* RW007 就绪标志 */
    bool rw007_int_high;           /* RW007 中断脚电平 */
    uint32_t rw007_reset_count;    /* 复位启动次数，供诊断 */

    uint8_t led_ring_mode;         /* 0=off, 1=center, 2=inner, 3=outer */
    uint8_t led_ring_pixel_count;  /* 固定为 19，便于 UI 自检 */
    board_gpio_port_snapshot_t gpio[BOARD_GPIO_PORT_COUNT]; /* GPIOA..GPIOI */
} board_service_snapshot_t;

/**
 * 任务层送给服务层的一次性请求。服务层处理后会把对应字段清零。
 * 这个结构只描述消息，不包含 RT-Thread 线程、锁或队列。
 */
typedef struct
{
    bool refresh;       /* 立即刷新光感/存储等慢速设备，不等 500 ms 周期 */
    bool rw007_reset;   /* 释放复位并启动 RW007 WiFi 模组 */
    bool attitude_zero; /* 将当前方向设为正方向（零方向） */
    bool led_ring_next; /* 切换到下一组 LED：关/中心/内环/外环 */
} board_service_requests_t;

/**
 * @brief  初始化所有板载驱动和算法状态，并写入第一份快照。
 * @param  snapshot 任务层持有的快照缓冲区，内部先整体清零再填充。
 * @note   开机刻意保持 RW007 处于复位，规避上电频闪；仅 Network 页面命令才启动它。
 */
void board_service_init(board_service_snapshot_t *snapshot);
/**
 * @brief  执行一轮采集/融合/状态更新；应由任务层每 20 ms 调用。
 * @param  snapshot 输入输出快照。
 * @param  requests 一次性请求集合，允许为 NULL；处理后字段被清零，每条请求只生效一次。
 * @note   姿态积分步长 dt 由 rt_tick 实测，慢速设备每 500 ms 或收到 refresh 时刷新。
 */
void board_service_process(board_service_snapshot_t *snapshot,
                           board_service_requests_t *requests);
/**
 * @brief  服务建议的任务循环周期。
 * @retval 周期毫秒数，供 rt_thread_mdelay 使用。
 */
uint32_t board_service_period_ms(void);

#endif /* BOARD_SERVICE_H */
