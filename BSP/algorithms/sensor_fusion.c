/**
 * @file sensor_fusion.c
 * @brief 閫傚悎 Cortex-M4F 鐨勮交閲忓Э鎬佺畻娉曘€? *
 * 鏁版嵁娴侊細ICM20608 鍘熷鍔犻€熷害/瑙掗€熷害/鑺墖娓╁害 -> 寮€鏈洪潤姝㈤浂鍋忔牎鍑?->
 * Mahony 浜掕ˉ婊ゆ尝绉垎鍥涘厓鏁?-> 鐩稿闆舵柟鍚戞鎷夎锛堝啀缁忎竴缁?Kalman 骞虫粦锛夈€? *
 * 濮挎€佷富浣撻噰鐢ㄥ叚杞?Mahony 闈炵嚎鎬т簰琛ユ护娉細闄€铻轰华璐熻矗蹇€熸棆杞紝
 * 鍔犻€熷害璁＄粰鍑洪噸鍔涙柟鍚戝苟闀挎湡绾︽潫 roll/pitch銆傝緭鍑烘鎷夎鍐嶇粡杩囦笁涓? * 鐙珛鐨勪竴缁?Kalman 婊ゆ尝鍣紝浠呯敤浜庢姂鍒舵樉绀烘姈鍔ㄣ€? *
 * ICM20608 娌℃湁纾佸姏璁★紝鍥犳 yaw 娌℃湁缁濆鍦扮鍙傝€冦€傞潤姝㈤浂閫熸洿鏂拌兘
 * 鏄捐憲闄嶄綆 yaw 闆舵紓锛屼絾鏃犳硶浠庣墿鐞嗕笂娑堥櫎杩愬姩杩囩▼涓殑闀挎湡鑸悜婕傜Щ銆? */
#include "sensor_fusion.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define DEG_TO_RAD                 0.01745329251994329577f
#define RAD_TO_DEG                 57.295779513082320876f
#define CALIBRATION_GYRO_LIMIT_DPS 8.0f    /* 鏍″噯鏈熻閫熷害骞呭€间笂闄愶紝瓒呰繃鍗冲垽瀹氭澘瀛愬湪鍔?*/
#define STATIONARY_GYRO_LIMIT_DPS  0.8f    /* 杩愯鏈熼潤姝㈠垽瀹氱殑瑙掗€熷害闃堝€?*/
#define MAHONY_TWO_KP              3.6f    /* Mahony 姣斾緥澧炵泭锛?*Kp锛夛紝鍐冲畾閲嶅姏鏍℃鏀舵暃閫熷害 */
#define MAHONY_TWO_KI              0.070f  /* Mahony 绉垎澧炵泭锛?*Ki锛夛紝鍚告敹闄€铻烘畫浣欓浂鍋?*/
#define BIAS_TRACK_ALPHA           0.0100f /* 闈欐闆跺亸璺熻釜鐨勪竴闃朵綆閫氱郴鏁帮紝瓒婂皬瓒婄ǔ */
#define TEMP_LEARN_DELTA_C         0.50f   /* 瑙﹀彂涓€娆℃俯婕傛枩鐜囧涔犳墍闇€鐨勬渶灏忔俯搴﹂棿闅?*/
#define TEMP_COEFF_ALPHA           0.15f   /* 娓╂紓绯绘暟浼拌鍊肩殑浣庨€氱郴鏁?*/
#define TEMP_COEFF_LIMIT_DPS_C     0.20f   /* 娓╂紓绯绘暟闄愬箙锛岄槻姝㈣瀛︿範瀵艰嚧妯″瀷鍙戞暎 */

/* 鏈湴闄愬箙宸ュ叿銆?*/
static float clampf_local(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

/* 涓夌淮鍚戦噺妯￠暱銆?*/
static float vector_norm3(const float vector[3])
{
    return sqrtf(vector[0] * vector[0] + vector[1] * vector[1] +
                 vector[2] * vector[2]);
}

/* Normalize quaternion to unit magnitude.
 *
 * Quaternion drift accumulates in fusion loop due to numerical errors.
 * Every iteration, magnitude may deviate from 1.0. Normalization ensures
 * the quaternion remains valid: ||q|| = 1 means q represents a true rotation.
 *
 * Guard against numerical collapse: if norm < 1e-6, reset to identity (1,0,0,0).
 */
static void quaternion_normalize(float q[4])
{
    float norm = sqrtf(q[0] * q[0] + q[1] * q[1] +
                       q[2] * q[2] + q[3] * q[3]);
    if (norm < 1.0e-6f)
    {
        q[0] = 1.0f; q[1] = 0.0f; q[2] = 0.0f; q[3] = 0.0f;
        return;
    }
    norm = 1.0f / norm;
    for (uint8_t i = 0U; i < 4U; ++i) q[i] *= norm;
}

/* 鍝堝瘑椤垮洓鍏冩暟涔樻硶 out = a 鈯?b锛岀敤浜庢眰鐩稿濮挎€併€?*/
static void quaternion_multiply(const float a[4], const float b[4], float out[4])
{
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    out[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

static void quaternion_from_accel(const float accel_mg[3], float q[4])
{
    float roll = atan2f(accel_mg[1], accel_mg[2]);
    float pitch = atan2f(-accel_mg[0],
                         sqrtf(accel_mg[1] * accel_mg[1] +
                               accel_mg[2] * accel_mg[2]));
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);

    /* 寮€鏈鸿埅鍚戝畾涔変负 0锛涘叚杞?IMU 鏃犳硶浠庨噸鍔涚‘瀹?yaw銆?*/
    q[0] = cr * cp;
    q[1] = sr * cp;
    q[2] = cr * sp;
    q[3] = -sr * sp;
    quaternion_normalize(q);
}

/* 鎶婃祴閲忚灞曞紑鍒板弬鑰冨€?卤180掳 閭诲煙锛岄伩鍏?Kalman 鍦?卤180掳 璺ㄧ晫澶勬潵鍥炶烦鍙樸€?*/
static float angle_unwrap(float measurement, float reference)
{
    while ((measurement - reference) > 180.0f) measurement -= 360.0f;
    while ((measurement - reference) < -180.0f) measurement += 360.0f;
    return measurement;
}

/* 鎶樺彔鍥?(-180, 180] 鍖洪棿锛屼緵 UI 鏄剧ず銆?*/
static float angle_wrap(float angle)
{
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

/**
 * @brief 鍒濆鍖栦竴缁?Kalman 婊ゆ尝鍣ㄣ€? * @param filter            婊ゆ尝鍣ㄧ姸鎬併€? * @param process_noise     杩囩▼鍣０ Q锛岃秺澶ц窡闅忚秺蹇€? * @param measurement_noise 娴嬮噺鍣０ R锛岃秺澶ц緭鍑鸿秺骞虫粦銆? */
void scalar_kalman_init(scalar_kalman_t *filter,
                        float process_noise, float measurement_noise)
{
    if (filter == NULL) return;
    filter->estimate = 0.0f;
    filter->covariance = 1.0f;
    filter->process_noise = process_noise;
    filter->measurement_noise = measurement_noise;
    filter->initialized = false;
}

/**
 * @brief 閫佸叆涓€娆℃祴閲忓苟杩斿洖婊ゆ尝浼拌銆? * @param filter      婊ゆ尝鍣ㄧ姸鎬併€? * @param measurement 鏈娴嬮噺鍊笺€? * @retval 婊ゆ尝鍚庣殑浼拌鍊硷紱filter 涓?NULL 鏃跺師鏍疯繑鍥炴祴閲忓€笺€? * @note  棣栨璋冪敤鐩存帴鐢ㄦ祴閲忓€煎垵濮嬪寲锛岄伩鍏嶄粠 0 鏀舵暃鐨勫惎鍔ㄦ嫋灏俱€? */
float scalar_kalman_update(scalar_kalman_t *filter, float measurement)
{
    float gain;
    if (filter == NULL) return measurement;
    if (!filter->initialized)
    {
        filter->estimate = measurement;
        filter->covariance = 1.0f;
        filter->initialized = true;
        return measurement;
    }

    filter->covariance += filter->process_noise;
    gain = filter->covariance /
           (filter->covariance + filter->measurement_noise);
    filter->estimate += gain * (measurement - filter->estimate);
    filter->covariance *= (1.0f - gain);
    return filter->estimate;
}

/**
 * @brief 娓呴櫎浼拌鐘舵€侊紝涓嬩竴娆?update 閲嶆柊鍒濆鍖栥€? * @param filter 婊ゆ尝鍣ㄧ姸鎬併€? */
void scalar_kalman_reset(scalar_kalman_t *filter)
{
    if (filter == NULL) return;
    filter->estimate = 0.0f;
    filter->covariance = 1.0f;
    filter->initialized = false;
}

/**
 * @brief 澶嶄綅濮挎€佹护娉㈠櫒锛氬垵濮嬪Э鎬佷笌闆舵柟鍚戝弬鑰冨潎涓哄崟浣嶅洓鍏冩暟锛岀瓑寰呭紑鏈烘牎鍑嗐€? * @param filter 婊ゆ尝鍣ㄧ姸鎬併€? */
void attitude_filter_init(attitude_filter_t *filter)
{
    if (filter == NULL) return;
    memset(filter, 0, sizeof(*filter));
    filter->q[0] = 1.0f;
    filter->reference[0] = 1.0f;
    for (uint8_t i = 0U; i < 3U; ++i)
        scalar_kalman_init(&filter->angle_filter[i], 0.08f, 1.5f);
}

/**
 * @brief 寮€鏈烘牎鍑嗭細绱Н杩炵画闈欐鏍锋湰锛屾眰闄€铻洪浂鍋忋€佹俯搴﹂敋鐐逛笌鍒濆濮挎€併€? * @param filter        婊ゆ尝鍣ㄧ姸鎬併€? * @param accel_mg      鍔犻€熷害锛宮g锛涘箙鍊奸』鎺ヨ繎 1 g 鎵嶈鍏ユ牱鏈€? * @param gyro_dps      瑙掗€熷害锛宒ps锛涘箙鍊艰秴闄愬垯鍒ゅ畾闈為潤姝㈠苟娓呴浂閲嶈銆? * @param temperature_c 鑺墖娓╁害锛岀敤浜庡缓绔嬮浂鍋忕殑娓╁害閿氱偣銆? * @retval true 琛ㄧず宸叉敀婊¤繛缁潤姝㈡牱鏈苟瀹屾垚鏍″噯銆? */
bool attitude_filter_calibrate(attitude_filter_t *filter,
                               const float accel_mg[3],
                               const float gyro_dps[3],
                               float temperature_c)
{
    float accel_norm;
    float gyro_norm;
    if ((filter == NULL) || (accel_mg == NULL) || (gyro_dps == NULL)) return false;
    if (filter->ready) return true;

    accel_norm = vector_norm3(accel_mg);
    gyro_norm = vector_norm3(gyro_dps);
    /* 鏍锋湰璐ㄩ噺闂ㄦ锛氬彧鏈夋帴杩戠函閲嶅姏涓斿嚑涔庝笉杞姩鐨勫抚鎵嶅弬涓庡钩鍧囷紝杩愬姩涓竻闆堕噸璁°€?*/
    if ((accel_norm < 850.0f) || (accel_norm > 1150.0f) ||
        (gyro_norm > CALIBRATION_GYRO_LIMIT_DPS))
    {
        filter->calibration_samples = 0U;
        memset(filter->calibration_sum, 0, sizeof(filter->calibration_sum));
        filter->calibration_temperature_sum = 0.0f;
        return false;
    }

    for (uint8_t i = 0U; i < 3U; ++i)
        filter->calibration_sum[i] += gyro_dps[i];
    filter->calibration_temperature_sum += temperature_c;
    ++filter->calibration_samples;

    if (filter->calibration_samples >= ATTITUDE_CALIBRATION_SAMPLE_COUNT)
    {
        for (uint8_t i = 0U; i < 3U; ++i)
        {
            filter->gyro_bias_dps[i] = filter->calibration_sum[i] /
                                       (float)filter->calibration_samples;
            filter->temperature_anchor_bias[i] = filter->gyro_bias_dps[i];
        }
        filter->bias_temperature_c = filter->calibration_temperature_sum /
                                     (float)filter->calibration_samples;
        filter->temperature_anchor_c = filter->bias_temperature_c;
        filter->temperature_ready = true;
        quaternion_from_accel(accel_mg, filter->q);
        memcpy(filter->reference, filter->q, sizeof(filter->q));
        filter->reference_valid = true;
        filter->ready = true;
        filter->stationary = true;
    }
    return filter->ready;
}

/**
 * @brief 涓昏瀺鍚堬細娓╁害鍓嶉琛ュ伩闆跺亸锛屽啀鐢?Mahony 浜掕ˉ婊ゆ尝绉垎鍥涘厓鏁般€? * @param filter        婊ゆ尝鍣ㄧ姸鎬侊紝椤诲凡瀹屾垚鏍″噯锛坮eady锛夈€? * @param accel_mg      鍔犻€熷害锛宮g锛涗粎骞呭€兼帴杩?1 g 鏃跺弬涓庨噸鍔涙牎姝ｃ€? * @param gyro_dps      鍘熷瑙掗€熷害锛宒ps銆? * @param temperature_c 鑺墖娓╁害锛岄┍鍔ㄩ浂鍋忔俯婕傚墠棣堜笌鏂滅巼瀛︿範銆? * @param dt_s          璺濅笂娆¤皟鐢ㄧ殑鐪熷疄闂撮殧锛屽唴閮ㄩ檺骞呭埌 2~50 ms 瀹夊叏鑼冨洿銆? */
void attitude_filter_update(attitude_filter_t *filter,
                            const float accel_mg[3],
                            const float gyro_dps[3],
                            float temperature_c,
                            float dt_s)
{
    float accel[3];
    float gyro[3];
    float accel_norm;
    float gyro_norm;
    float q0, q1, q2, q3;
    float ex, ey, ez;

    if ((filter == NULL) || !filter->ready ||
        (accel_mg == NULL) || (gyro_dps == NULL)) return;

    dt_s = clampf_local(dt_s, 0.002f, 0.05f);
    accel_norm = vector_norm3(accel_mg);

    /*
     * 鍏堟牴鎹凡瀛︿範鍒扮殑 dps/掳C 绯绘暟锛屾妸闆跺亸浠庝笂涓€甯ф俯搴﹀钩绉诲埌褰撳墠娓╁害銆?     * 闄愬畾娓╁害鑼冨洿鍙伩鍏嶉€氫俊寮傚父鍊奸€犳垚涓€娆℃€уぇ骞呬慨姝ｃ€?     */
    if (filter->temperature_ready &&
        (temperature_c > -20.0f) && (temperature_c < 85.0f))
    {
        float temperature_step = temperature_c - filter->bias_temperature_c;
        temperature_step = clampf_local(temperature_step, -1.0f, 1.0f);
        for (uint8_t i = 0U; i < 3U; ++i)
            filter->gyro_bias_dps[i] += filter->gyro_temp_coeff[i] * temperature_step;
        filter->bias_temperature_c += temperature_step;
    }
    for (uint8_t i = 0U; i < 3U; ++i)
        gyro[i] = gyro_dps[i] - filter->gyro_bias_dps[i];
    gyro_norm = vector_norm3(gyro);

    if ((accel_norm > 970.0f) && (accel_norm < 1030.0f) &&
        (gyro_norm < STATIONARY_GYRO_LIMIT_DPS))
    {
        if (filter->stationary_samples < 1000U) ++filter->stationary_samples;
    }
    else
    {
        filter->stationary_samples = 0U;
    }
    filter->stationary = filter->stationary_samples >= 50U;

    /* 闈欐瓒呰繃 1 s 鍚庡仛闆堕€熷亸缃窡韪紝璺熼殢鍣ㄤ欢鍗囨俯閫犳垚鐨勬參婕傘€?*/
    if (filter->stationary)
    {
        for (uint8_t i = 0U; i < 3U; ++i)
        {
            filter->gyro_bias_dps[i] += BIAS_TRACK_ALPHA *
                                         (gyro_dps[i] - filter->gyro_bias_dps[i]);
            gyro[i] = gyro_dps[i] - filter->gyro_bias_dps[i];
        }

        /*
         * 娓╁害鐩稿涓婁竴涓涔犵偣鍙樺寲鑷冲皯 0.5掳C 鏃讹紝浼扮畻涓€娆￠浂鍋忔俯搴︽枩鐜囥€?         * 鏂滅巼缁忚繃浣庨€氬苟闄愬埗鍦?卤0.20 dps/掳C锛岄槻姝㈡尟鍔ㄦ垨璇垽闈欐浣挎ā鍨嬪彂鏁ｃ€?         * 瀛﹀埌绯绘暟鍚庯紝鍗充娇寮€鍙戞澘闅忓悗杩愬姩锛屼篃鍙寜娓╁害鍓嶉淇闆跺亸銆?         */
        if (filter->temperature_ready &&
            (fabsf(temperature_c - filter->temperature_anchor_c) >= TEMP_LEARN_DELTA_C))
        {
            float temperature_delta = temperature_c - filter->temperature_anchor_c;
            for (uint8_t i = 0U; i < 3U; ++i)
            {
                float observed = (filter->gyro_bias_dps[i] -
                                  filter->temperature_anchor_bias[i]) /
                                 temperature_delta;
                observed = clampf_local(observed, -TEMP_COEFF_LIMIT_DPS_C,
                                        TEMP_COEFF_LIMIT_DPS_C);
                filter->gyro_temp_coeff[i] += TEMP_COEFF_ALPHA *
                    (observed - filter->gyro_temp_coeff[i]);
                filter->temperature_anchor_bias[i] = filter->gyro_bias_dps[i];
            }
            filter->temperature_anchor_c = temperature_c;
            filter->temperature_compensation_ready = true;
        }
    }

    for (uint8_t i = 0U; i < 3U; ++i)
        filter->corrected_gyro_dps[i] = gyro[i];

    gyro[0] *= DEG_TO_RAD;
    gyro[1] *= DEG_TO_RAD;
    gyro[2] *= DEG_TO_RAD;
    q0 = filter->q[0]; q1 = filter->q[1];
    q2 = filter->q[2]; q3 = filter->q[3];

    /* 鍙湁鍔犻€熷害骞呭€兼帴杩戦噸鍔涙椂鎵嶄娇鐢ㄩ噸鍔涙牎姝ｏ紝閬垮厤杩愬姩鍔犻€熷害璇濮挎€併€?*/
    if ((accel_norm > 850.0f) && (accel_norm < 1150.0f))
    {
        float reciprocal = 1.0f / accel_norm;
        accel[0] = accel_mg[0] * reciprocal;
        accel[1] = accel_mg[1] * reciprocal;
        accel[2] = accel_mg[2] * reciprocal;

        /* 娴嬪緱閲嶅姏鏂瑰悜涓庡洓鍏冩暟棰勬祴閲嶅姏鏂瑰悜鐨勫弶绉紙Mahony half-error锛夈€?*/
        {
            float half_vx = q1 * q3 - q0 * q2;
            float half_vy = q0 * q1 + q2 * q3;
            float half_vz = q0 * q0 - 0.5f + q3 * q3;
            ex = accel[1] * half_vz - accel[2] * half_vy;
            ey = accel[2] * half_vx - accel[0] * half_vz;
            ez = accel[0] * half_vy - accel[1] * half_vx;
        }

        filter->integral_error[0] += MAHONY_TWO_KI * ex * dt_s;
        filter->integral_error[1] += MAHONY_TWO_KI * ey * dt_s;
        filter->integral_error[2] += MAHONY_TWO_KI * ez * dt_s;
        gyro[0] += MAHONY_TWO_KP * ex + filter->integral_error[0];
        gyro[1] += MAHONY_TWO_KP * ey + filter->integral_error[1];
        gyro[2] += MAHONY_TWO_KP * ez + filter->integral_error[2];
    }

    /* q_dot = 0.5 * q (x) omega锛岄殢鍚庡綊涓€鍖栭槻姝㈢Н鍒嗚宸牬鍧忓崟浣嶅洓鍏冩暟銆?*/
    filter->q[0] += 0.5f * (-q1 * gyro[0] - q2 * gyro[1] - q3 * gyro[2]) * dt_s;
    filter->q[1] += 0.5f * ( q0 * gyro[0] + q2 * gyro[2] - q3 * gyro[1]) * dt_s;
    filter->q[2] += 0.5f * ( q0 * gyro[1] - q1 * gyro[2] + q3 * gyro[0]) * dt_s;
    filter->q[3] += 0.5f * ( q0 * gyro[2] + q1 * gyro[1] - q2 * gyro[0]) * dt_s;
    quaternion_normalize(filter->q);
}

/**
 * @brief 灏嗗綋鍓嶆柟鍚戣涓烘柊鐨勬鏂瑰悜锛屽悓鏃舵竻闄や笁涓樉绀鸿搴︾殑 Kalman 鐘舵€併€? * @param filter 婊ゆ尝鍣ㄧ姸鎬侊紱鏈畬鎴愭牎鍑嗘椂璋冪敤鏃犳晥銆? * @note  澶嶄綅瑙掑害婊ゆ尝鍣ㄥ彲閬垮厤褰掗浂鍚庢樉绀哄€艰鏃т及璁℃嫋灏俱€? */
void attitude_filter_zero(attitude_filter_t *filter)
{
    if ((filter == NULL) || !filter->ready) return;
    memcpy(filter->reference, filter->q, sizeof(filter->q));
    filter->reference_valid = true;
    for (uint8_t i = 0U; i < 3U; ++i)
        scalar_kalman_reset(&filter->angle_filter[i]);
}

/**
 * @brief 璇诲彇鐩稿闆舵柟鍚戠殑濮挎€佺粨鏋溿€? * @param filter 婊ゆ尝鍣ㄧ姸鎬併€? * @param result 杈撳嚭锛氱浉瀵瑰洓鍏冩暟銆佹鎷夎銆佽ˉ鍋垮悗瑙掗€熷害涓庨浂鍋忎及璁°€? * @note  鐩稿濮挎€?= reference 鍏辫江 鈯?褰撳墠 q锛屾晠 yaw 鏄浉瀵瑰綊闆舵柟鍚戠殑鑸悜宸€? */
void attitude_filter_get(attitude_filter_t *filter,
                         attitude_result_t *result)
{
    float conjugate[4];
    float relative[4];
    float roll, pitch, yaw;

    if ((filter == NULL) || (result == NULL)) return;
    memset(result, 0, sizeof(*result));
    result->ready = filter->ready;
    result->stationary = filter->stationary;
    memcpy(result->gyro_bias_dps, filter->gyro_bias_dps,
           sizeof(result->gyro_bias_dps));
    memcpy(result->gyro_temp_coeff, filter->gyro_temp_coeff,
           sizeof(result->gyro_temp_coeff));
    memcpy(result->corrected_gyro_dps, filter->corrected_gyro_dps,
           sizeof(result->corrected_gyro_dps));
    result->temperature_compensation_ready =
        filter->temperature_compensation_ready;
    if (!filter->ready || !filter->reference_valid)
    {
        result->quaternion[0] = 1.0f;
        return;
    }

    conjugate[0] = filter->reference[0];
    conjugate[1] = -filter->reference[1];
    conjugate[2] = -filter->reference[2];
    conjugate[3] = -filter->reference[3];
    quaternion_multiply(conjugate, filter->q, relative);
    quaternion_normalize(relative);
    memcpy(result->quaternion, relative, sizeof(relative));

    roll = atan2f(2.0f * (relative[0] * relative[1] + relative[2] * relative[3]),
                  1.0f - 2.0f * (relative[1] * relative[1] + relative[2] * relative[2])) * RAD_TO_DEG;
    pitch = asinf(clampf_local(2.0f * (relative[0] * relative[2] -
                                      relative[3] * relative[1]), -1.0f, 1.0f)) * RAD_TO_DEG;
    yaw = atan2f(2.0f * (relative[0] * relative[3] + relative[1] * relative[2]),
                 1.0f - 2.0f * (relative[2] * relative[2] + relative[3] * relative[3])) * RAD_TO_DEG;

    roll = angle_unwrap(roll, filter->angle_filter[0].estimate);
    pitch = angle_unwrap(pitch, filter->angle_filter[1].estimate);
    yaw = angle_unwrap(yaw, filter->angle_filter[2].estimate);
    result->roll_deg = angle_wrap(scalar_kalman_update(&filter->angle_filter[0], roll));
    result->pitch_deg = angle_wrap(scalar_kalman_update(&filter->angle_filter[1], pitch));
    result->yaw_deg = angle_wrap(scalar_kalman_update(&filter->angle_filter[2], yaw));
}

