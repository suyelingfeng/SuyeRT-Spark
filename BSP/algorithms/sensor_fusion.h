/**
 * @file sensor_fusion.h
 * @brief 与具体传感器驱动无关的一维 Kalman 和六轴姿态融合。
 *
 * 输入：ICM20608 的加速度（mg）、角速度（dps）与芯片温度；
 * 输出：相对零方向的四元数、欧拉角，以及在线估计的陀螺零偏/温漂系数。
 *
 * 本模块不访问 I2C、RT-Thread 或 LVGL，便于以后移植到其他 BSP，
 * 也便于在 PC 上用记录数据单独验证算法。
 */
#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <stdbool.h>
#include <stdint.h>

/** 50 Hz 下为约 6 秒；扩大平均窗口以降低开机零偏估计噪声。 */
#define ATTITUDE_CALIBRATION_SAMPLE_COUNT 300U

/** 一维 Kalman 滤波器状态：用于温湿度与欧拉角的平滑显示。 */
typedef struct
{
    float estimate;          /* 当前状态估计值 */
    float covariance;        /* 估计协方差 P */
    float process_noise;     /* 过程噪声 Q，越大跟随越快 */
    float measurement_noise; /* 测量噪声 R，越大输出越平滑 */
    bool initialized;        /* 首帧已直接初始化的标志 */
} scalar_kalman_t;

/**
 * 姿态融合状态：开机校准求零偏与初始姿态，随后 Mahony 积分，
 * 静止期间持续做零偏跟踪与温漂斜率学习。
 */
typedef struct
{
    float q[4];                 /* 当前姿态四元数，w/x/y/z。 */
    float reference[4];         /* 用户零方向对应的参考四元数。 */
    float gyro_bias_dps[3];     /* 开机校准并在静止时缓慢跟踪的零偏。 */
    float gyro_temp_coeff[3];   /* 三轴温漂系数，单位 dps/摄氏度。 */
    float temperature_anchor_bias[3]; /* 上一个温度学习点的三轴零偏。 */
    float corrected_gyro_dps[3];/* 最近一帧扣除温度零偏后的角速度。 */
    float integral_error[3];    /* Mahony 积分反馈，用于慢速偏置补偿。 */
    float calibration_sum[3];           /* 校准期陀螺累加和。 */
    float calibration_temperature_sum;  /* 校准期温度累加和。 */
    float bias_temperature_c;  /* gyro_bias_dps 当前对应的芯片温度。 */
    float temperature_anchor_c;/* 上一个温度学习点的芯片温度。 */
    uint16_t calibration_samples;      /* 已累积的连续静止样本数。 */
    uint16_t stationary_samples;       /* 连续静止帧计数，满 50 帧才判静止。 */
    scalar_kalman_t angle_filter[3];   /* roll/pitch/yaw 的显示平滑滤波器。 */
    bool ready;                        /* 开机校准完成，姿态输出可信。 */
    bool reference_valid;              /* 零方向参考四元数有效。 */
    bool stationary;                   /* 当前判定为静止。 */
    bool temperature_ready;            /* 零偏温度锚点已建立。 */
    bool temperature_compensation_ready;/* 温漂系数已学到，可前馈补偿。 */
} attitude_filter_t;

/** 姿态解算输出：相对零方向的姿态与在线估计的零偏参数，供服务层打包进快照。 */
typedef struct
{
    float quaternion[4];        /* 相对开机/手动归零方向的 w/x/y/z。 */
    float roll_deg;             /* 相对横滚角，度。 */
    float pitch_deg;            /* 相对俯仰角，度。 */
    float yaw_deg;              /* 相对航向角，度；无磁力计，仅为相对值。 */
    float gyro_bias_dps[3];     /* 当前零偏估计，dps。 */
    float gyro_temp_coeff[3];   /* 温漂系数，dps/摄氏度。 */
    float corrected_gyro_dps[3];/* 扣除零偏后的角速度，dps。 */
    bool ready;                 /* 校准完成，结果可信。 */
    bool stationary;            /* 静止标志，供 UI 提示保持静止。 */
    bool temperature_compensation_ready; /* 温漂补偿已生效。 */
} attitude_result_t;

/**
 * @brief  初始化一维 Kalman 滤波器。
 * @param  filter            滤波器状态。
 * @param  process_noise     过程噪声 Q，越大跟随越快。
 * @param  measurement_noise 测量噪声 R，越大输出越平滑。
 */
void scalar_kalman_init(scalar_kalman_t *filter,
                        float process_noise, float measurement_noise);
/**
 * @brief  送入一次测量并返回滤波估计。
 * @param  filter      滤波器状态。
 * @param  measurement 本次测量值。
 * @retval 滤波后的估计值；filter 为 NULL 时原样返回测量值。
 * @note   首次调用直接用测量值初始化，避免从 0 收敛的启动拖尾。
 */
float scalar_kalman_update(scalar_kalman_t *filter, float measurement);
/**
 * @brief  清除估计状态，下一次 update 重新初始化。
 * @param  filter 滤波器状态。
 */
void scalar_kalman_reset(scalar_kalman_t *filter);

/**
 * @brief  复位姿态滤波器：初始姿态与零方向参考均为单位四元数，等待开机校准。
 * @param  filter 滤波器状态。
 */
void attitude_filter_init(attitude_filter_t *filter);

/**
 * @brief  开机校准阶段加入一帧样本。开发板必须保持静止；移动会重新计数。
 * @param  filter        滤波器状态。
 * @param  accel_mg      加速度，mg；幅值须接近 1 g 才计入样本。
 * @param  gyro_dps      角速度，dps；幅值超限则判定非静止并清零重计。
 * @param  temperature_c 芯片温度，用于建立零偏的温度锚点。
 * @retval true 表示已经完成连续静止样本校准并建立开机零方向。
 */
bool attitude_filter_calibrate(attitude_filter_t *filter,
                               const float accel_mg[3],
                               const float gyro_dps[3],
                               float temperature_c);

/**
 * @brief  主融合：温度前馈补偿零偏，再用 Mahony 互补滤波积分四元数。
 * @param  filter        滤波器状态，须已完成校准（ready）。
 * @param  accel_mg      加速度，mg；仅幅值接近 1 g 时参与重力校正。
 * @param  gyro_dps      原始角速度，dps。
 * @param  temperature_c 芯片温度，驱动零偏温漂前馈与斜率学习。
 * @param  dt_s          使用实际时间间隔更新四元数；内部限幅到 2~50 ms 安全范围。
 */
void attitude_filter_update(attitude_filter_t *filter,
                            const float accel_mg[3],
                            const float gyro_dps[3],
                            float temperature_c,
                            float dt_s);

/**
 * @brief  将当前方向设为新的正方向，同时清除三个显示角度的 Kalman 状态。
 * @param  filter 滤波器状态；未完成校准时调用无效。
 * @note   复位角度滤波器可避免归零后显示值被旧估计拖尾。
 */
void attitude_filter_zero(attitude_filter_t *filter);

/**
 * @brief  读取相对零方向的姿态结果。
 * @param  filter 滤波器状态。
 * @param  result 输出：相对四元数、欧拉角、补偿后角速度与零偏估计。
 * @note   相对姿态 = reference 共轭 ⊗ 当前 q，故 yaw 是相对归零方向的航向差。
 */
void attitude_filter_get(attitude_filter_t *filter,
                         attitude_result_t *result);

#endif /* SENSOR_FUSION_H */
