/**
 * @file board_service.c
 * @brief 灏嗙嫭绔嬮┍鍔ㄥ拰绠楁硶缁勫悎鎴?UI 鍙鐨勬澘绾х姸鎬佸揩鐓с€? *
 * 鏁版嵁娴侊細drivers 閲囬泦鍘熷鐗╃悊閲?-> algorithms 婊ゆ尝/濮挎€佽瀺鍚?->
 * 蹇収缁撴瀯 -> app_tasks.c 鍔犻攣鍙戝竷缁?LVGL/MSH锛涗竴娆℃€ц姹傚弽鍚戞祦鍏ユ湰鏂囦欢銆? *
 * 鍒嗗眰绾︽潫锛? * - drivers 鍙礋璐ｆ€荤嚎銆佸瘎瀛樺櫒銆佺墿鐞嗛噺鎹㈢畻锛? * - algorithms 鍙礋璐?Kalman 鍜屽洓鍏冩暟濮挎€佽瀺鍚堬紱
 * - 鏈枃浠跺喅瀹氶噰鏍峰懆鏈熷拰鍚勬ā鍧楃殑璋冪敤椤哄簭锛? * - RT-Thread 绾跨▼鍒涘缓銆佸叡浜暟鎹繚鎶ゅ拰娑堟伅鎻愪氦鍏ㄩ儴鍦?app_tasks.c銆? */
#include "board_service.h"
#include "soft_i2c.h"
#include "spi_driver.h"
#include "spi_config.h"
#include "spi2_adapter.h"
#include "aht21.h"
#include "ap3216c.h"
#include "icm20608.h"
#include "board_storage.h"
#include "rw007_hw.h"
#include "led_ring.h"
#include "gpio_monitor.h"
#include "sensor_fusion.h"
#include <rtthread.h>
#include <string.h>
#include "config.h"

/* 20 ms 蹇懆鏈熶繚璇佸Э鎬佺Н鍒嗘闀?dt 瓒冲灏忥紱500 ms 鎱㈠懆鏈熷彧鐢ㄤ簬鍏夋劅/瀛樺偍鎺㈡祴绛変綆棰戣澶囥€?*/

/* 涓ゆ潯杞欢 I2C 鎬荤嚎锛欰HT21 鐙崰涓€鏉★紱AP3216C 涓?ICM20608 鍏辩敤鍙︿竴鏉°€?*/
static const soft_i2c_bus_t aht_bus = {IO_AHT21_I2C_PORT, IO_AHT21_I2C_SDA_PIN, IO_AHT21_I2C_SCL_PIN};
static const soft_i2c_bus_t sensor_bus = {IO_SENSOR_I2C_PORT, IO_SENSOR_I2C_SDA_PIN, IO_SENSOR_I2C_SCL_PIN};

static aht21_t aht21;
static ap3216c_t ap3216c;
static icm20608_t icm20608;
static scalar_kalman_t temperature_filter;
static scalar_kalman_t humidity_filter;
static attitude_filter_t orientation_filter;
static rt_tick_t previous_imu_tick;
static rt_tick_t previous_slow_tick;
/* 鏍″噯瀹屾垚鍓嶆敹鍒扮殑褰掗浂璇锋眰涓嶈兘涓㈠け锛屽畬鎴愭牎鍑嗗悗鑷姩鎵ц銆?*/
static bool attitude_zero_pending;

static void update_gpio_snapshot(board_service_snapshot_t *snapshot)
{
    gpio_monitor_port_t ports[GPIO_MONITOR_PORT_COUNT];
    gpio_monitor_sample(ports);
    for (uint8_t i = 0U; i < BOARD_GPIO_PORT_COUNT; ++i)
    {
        snapshot->gpio[i].input = ports[i].input;
        snapshot->gpio[i].output = ports[i].output;
        snapshot->gpio[i].mode = ports[i].mode;
    }
}

/* 娴偣鐗╃悊閲忔寜鍊嶇巼瀹氱偣鍖栦负 int16锛氶ケ鍜岄槻婧㈠嚭锛屄?.5 鍋忕Щ瀹炵幇鍥涜垗浜斿叆銆?*/
/* Convert float physical quantity to fixed-point int16 with saturation and rounding.
 *
 * Fixed-point encoding strategy:
 *   - Avoids floating-point overhead in UI and storage
 *   - scale parameter defines the multiplier (e.g., 10.0 for tenths, 100.0 for hundredths)
 *   - Real value = encoded_value / scale (e.g., temperature_x10=250 -> 25.0 C)
 *   - Saturation clamps overflow to int16 range [-32768, 32767]
 *   - ±0.5 offset implements round-half-up for cleaner conversions
 *
 * Examples:
 *   - Temperature 25.3 C with scale 10.0 -> (25.3 * 10 + 0.5) = 254 (x10 format)
 *   - Gyro 1234.56 dps with scale 10.0 -> (1234.56 * 10 + 0.5) = 12345 (x10 format)
 */
static int16_t float_to_i16_scaled(float value, float scale)
{
    float scaled = value * scale;
    if (scaled > 32767.0f) return 32767;
    if (scaled < -32768.0f) return -32768;
    return (int16_t)(scaled + (scaled >= 0.0f ? 0.5f : -0.5f));
}

/* 璇诲彇 AHT21 娓╂箍搴﹀苟鍋氫竴缁?Kalman 骞虫粦锛沺oll 杩斿洖 0 琛ㄧず杞崲鏈畬鎴愶紝鏈抚淇濈暀鏃у€笺€?*/
static void update_environment(board_service_snapshot_t *snapshot)
{
    aht21_sample_t sample;
    float filtered;
    int result = aht21_poll(&aht21, &sample);
    if (result == 0) return;
    snapshot->aht21_ok = result > 0;
    if (result < 0) return;

    snapshot->temperature_x10 = sample.temperature_x10;
    snapshot->humidity_x10 = sample.humidity_x10;
    filtered = scalar_kalman_update(&temperature_filter,
                                    (float)sample.temperature_x10 * 0.1f);
    snapshot->temperature_kalman_x10 = float_to_i16_scaled(filtered, 10.0f);
    filtered = scalar_kalman_update(&humidity_filter,
                                    (float)sample.humidity_x10 * 0.1f);
    /* 婀垮害鐗╃悊鑼冨洿 0~100% RH锛孠alman 杈撳嚭鍙兘瓒婄晫锛岄挸浣嶅悗鍐嶅畾鐐瑰寲銆?*/
    if (filtered < 0.0f) filtered = 0.0f;
    if (filtered > 100.0f) filtered = 100.0f;
    snapshot->humidity_kalman_x10 = (uint16_t)(filtered * 10.0f + 0.5f);
    snapshot->environment_filter_ready = temperature_filter.initialized &&
                                         humidity_filter.initialized;
}

/* 璇诲彇 ICM20608锛氭湭瀹屾垚鏍″噯鏃剁疮绉浂鍋忔牱鏈紝瀹屾垚鍚庡仛濮挎€佽瀺鍚堝苟濉厖蹇収銆?*/
static void update_attitude(board_service_snapshot_t *snapshot, float dt_s,
                            bool zero_requested)
{
    icm20608_sample_t imu;
    float accel_mg[3];
    attitude_result_t result;
    snapshot->icm20608_ok = icm20608_read(&icm20608, &imu);
    if (!snapshot->icm20608_ok) return;

    for (uint8_t i = 0U; i < 3U; ++i)
    {
        snapshot->accel_mg[i] = imu.accel_mg[i];
        /* 鏍″噯瀹屾垚鍓嶄笉瀛樺湪鍙潬琛ュ伩鍊硷紝鍥犳鍏叡蹇収淇濇寔 0锛屼笉鏄剧ず raw銆?*/
        if (!orientation_filter.ready) snapshot->gyro_dps_x10[i] = 0;
        accel_mg[i] = (float)imu.accel_mg[i];
    }
    snapshot->imu_temperature_x10 = imu.temperature_x10;

    if (!orientation_filter.ready)
        (void)attitude_filter_calibrate(&orientation_filter, accel_mg,
                                        imu.gyro_dps,
                                        (float)imu.temperature_x10 * 0.1f);
    else
        attitude_filter_update(&orientation_filter, accel_mg, imu.gyro_dps,
                               (float)imu.temperature_x10 * 0.1f, dt_s);

    if (zero_requested && orientation_filter.ready)
    {
        attitude_filter_zero(&orientation_filter);
        rt_kprintf("[IMU] Current orientation set as positive zero direction.\n");
    }

    /* Extract fusion results and convert to fixed-point snapshot fields.
     *
     * Quaternion representation (w, x, y, z):
     *   - W (scalar) is real part; xyz is vector part
     *   - Unit quaternion encodes 3D rotation: v_body = q * v_world * q_conjugate
     *   - Stored as q_x10000 (e.g., q=0.7071 -> 7071) for compact representation
     *
     * Euler angles (roll, pitch, yaw) derived from quaternion:
     *   - Roll: rotation around X-axis (±180 deg)
     *   - Pitch: rotation around Y-axis (±90 deg)
     *   - Yaw: rotation around Z-axis (±180 deg, relative, no magnetic reference)
     *   - Stored as x10 format (e.g., 45.3 deg -> 453) */
    attitude_filter_get(&orientation_filter, &result);
    snapshot->attitude_ready = result.ready;
    snapshot->attitude_stationary = result.stationary;
    snapshot->attitude_calibration_samples = orientation_filter.calibration_samples;
    snapshot->attitude_calibration_target = ATTITUDE_CALIBRATION_SAMPLE_COUNT;
    snapshot->gyro_temperature_compensation_ready =
        result.temperature_compensation_ready;
    
    /* Store quaternion (4 components, each x10000 fixed-point) */
    for (uint8_t i = 0U; i < 4U; ++i)
        snapshot->quaternion_x10000[i] =
            float_to_i16_scaled(result.quaternion[i], 10000.0f);
    
    /* Store Euler angles (x10 fixed-point for 0.1 degree precision) */
    snapshot->roll_x10 = float_to_i16_scaled(result.roll_deg, 10.0f);
    snapshot->pitch_x10 = float_to_i16_scaled(result.pitch_deg, 10.0f);
    snapshot->yaw_x10 = float_to_i16_scaled(result.yaw_deg, 10.0f);
    
    /* Store gyroscope bias and temperature compensation coefficients (x1000, x10000 scales) */
    for (uint8_t i = 0U; i < 3U; ++i)
    {
        /* Bias-corrected angular velocity: gyro_raw - bias(T) where T is temperature */
        if (result.ready)
            snapshot->gyro_dps_x10[i] =
                float_to_i16_scaled(result.corrected_gyro_dps[i], 10.0f);
        /* Learned gyro bias for this axis (dps per axis) */
        snapshot->gyro_bias_x1000[i] =
            float_to_i16_scaled(result.gyro_bias_dps[i], 1000.0f);
        /* Temperature sensitivity of gyro bias: how bias changes with temperature (dps/C) */
        snapshot->gyro_temp_coeff_x10000[i] =
            float_to_i16_scaled(result.gyro_temp_coeff[i], 10000.0f);
    }
}

/* 浣庨璁惧杞锛氬厜鎰熴€乄25Q/SD 鎺㈡祴锛屽苟瑙﹀彂涓嬩竴娆?AHT21 杞崲銆?*/
static void update_slow_devices(board_service_snapshot_t *snapshot)
{
    board_storage_status_t storage;
    snapshot->ap3216c_ok =
        ap3216c_read_light(&ap3216c, &snapshot->ambient_light_x10);
    snapshot->proximity = 0U; /* 涓洪伩鍏嶇數婧愭壈鍔紝PS/IR 鍔熻兘淇濇寔鍏抽棴銆?*/

    board_storage_probe(&storage);
    snapshot->flash_ok = storage.flash_ok;
    memcpy(snapshot->flash_jedec, storage.flash_jedec,
           sizeof(snapshot->flash_jedec));
    snapshot->flash_size_kib = storage.flash_size_kib;
    snapshot->sd_inserted = storage.sd_inserted;
    /* AHT21 杞崲鑰楁椂杈冮暱锛氳繖閲屽彧瑙﹀彂锛屼笅涓參鍛ㄦ湡鍐嶈鍙栵紝閬垮厤闃诲 20 ms 蹇懆鏈熴€?*/
    if (!aht21_is_pending(&aht21)) snapshot->aht21_ok = aht21_start(&aht21);
}

/* 鏌ヨ RW007 澶嶄綅/灏辩华鐘舵€侊紱鏀跺埌璇锋眰鏃舵墽琛屽浣嶉噸鍚祦绋嬪苟绱娆℃暟銆?*/
static void update_rw007(board_service_snapshot_t *snapshot, bool start_requested)
{
    rw007_hw_status_t status = {
        snapshot->rw007_reset_released,
        snapshot->rw007_ready,
        snapshot->rw007_int_high
    };
    if (start_requested)
    {
        rw007_hw_reset_and_start(&status);
        ++snapshot->rw007_reset_count;
    }
    else
    {
        rw007_hw_read_status(&status);
    }
    snapshot->rw007_reset_released = status.reset_released;
    snapshot->rw007_ready = status.ready;
    snapshot->rw007_int_high = status.int_high;
}

/**
 * @brief 鍒濆鍖栨澘杞戒紶鎰熷櫒銆佹护娉㈠櫒涓庡瓨鍌紝骞跺啓鍏ラ浠藉揩鐓с€? * @param snapshot 杈撳嚭蹇収缂撳啿鍖猴紝鐢变换鍔″眰鎸佹湁銆? * @note  寮€鏈哄埢鎰忎繚鎸?RW007 澶嶄綅锛岃閬夸笂鐢甸闂紱Network 椤甸潰鍛戒护鎵嶄細鍚姩瀹冦€? */
void board_service_init(board_service_snapshot_t *snapshot)
{
    if (snapshot == RT_NULL) return;
    memset(snapshot, 0, sizeof(*snapshot));
    soft_i2c_timebase_init();
    soft_i2c_bus_init(&aht_bus);
    soft_i2c_bus_init(&sensor_bus);
    spi_driver_init();
    spi2_adapter_init();
    led_ring_init();
    gpio_monitor_init();
    scalar_kalman_init(&temperature_filter, KALMAN_TEMPERATURE_Q, KALMAN_TEMPERATURE_R);
    scalar_kalman_init(&humidity_filter, KALMAN_HUMIDITY_Q, KALMAN_HUMIDITY_R);
    attitude_filter_init(&orientation_filter);
    attitude_zero_pending = false;
    snapshot->aht21_ok = aht21_init(&aht21, &aht_bus);
    snapshot->ap3216c_ok = ap3216c_init_als_only(&ap3216c, &sensor_bus);
    snapshot->icm20608_ok = icm20608_init(&icm20608, &sensor_bus);
    update_slow_devices(snapshot);
    snapshot->led_ring_mode = (uint8_t)led_ring_get_mode();
    snapshot->led_ring_pixel_count = LED_RING_PIXEL_COUNT;
    update_gpio_snapshot(snapshot);

    /* 棰戦棯瑙勯伩绛栫暐锛氬紑鏈轰笉鍚姩 RW007锛孨etwork 椤甸潰鍛戒护鎵嶄細閲婃斁澶嶄綅銆?*/
    snapshot->rw007_reset_released = false;
    snapshot->rw007_ready = false;
    snapshot->rw007_int_high = false;
    previous_imu_tick = rt_tick_get();
    previous_slow_tick = previous_imu_tick;
    rt_kprintf("[BSP] Sensors, filters, W25Q and SD ready; RW007 held in reset.\n");
    rt_kprintf("[IMU] Keep board still for 6 s / 300 samples; temperature bias learning enabled.\n");
}

/**
 * @brief 鎵ц涓€杞噰闆?铻嶅悎/鎱㈤€熻澶囪皟搴︼紝骞舵秷璐逛竴娆℃€ц姹傘€? * @param snapshot  杈撳叆杈撳嚭蹇収銆? * @param requests  涓€娆℃€ц姹傞泦鍚堬紝鍏佽涓?NULL锛涘鐞嗗悗娓呴浂锛屼繚璇佹瘡鏉¤姹傚彧鐢熸晥涓€娆°€? * @note  dt 鐢?rt_tick 瀹炴祴锛屽蹇嶄换鍔¤皟搴︽姈鍔紱鎱㈤€熻澶囨瘡 500 ms 鎴?refresh 璇锋眰鏃跺埛鏂般€? */
void board_service_process(board_service_snapshot_t *snapshot,
                           board_service_requests_t *requests)
{
    rt_tick_t now;
    float dt_s;
    bool refresh = false;
    bool rw007_reset = false;
    bool attitude_zero = false;
    bool led_next = false;
    if (snapshot == RT_NULL) return;
    if (requests != RT_NULL)
    {
        refresh = requests->refresh;
        rw007_reset = requests->rw007_reset;
        attitude_zero = requests->attitude_zero;
        led_next = requests->led_ring_next;
        /* 璇锋眰鏄?涓€娆℃€ф秷鎭?锛氳瀹屽嵆娓呴浂锛岄伩鍏嶅悓涓€璇锋眰琚悗缁懆鏈熼噸澶嶆墽琛屻€?*/
        memset(requests, 0, sizeof(*requests));
    }

    now = rt_tick_get();
    /* dt 鐢ㄨ妭鎷嶅疄娴嬭€岄潪鍋囧畾 20 ms锛氫换鍔¤楂樹紭鍏堢骇鎶㈠崰鏃剁Н鍒嗘闀夸粛鐒舵纭€?*/
    dt_s = (float)(now - previous_imu_tick) / (float)RT_TICK_PER_SECOND;
    previous_imu_tick = now;
    update_environment(snapshot);
    if (attitude_zero) attitude_zero_pending = true;
    update_attitude(snapshot, dt_s, attitude_zero_pending);
    /* 鏍″噯瀹屾垚鍚庡綊闆跺凡鍦?update_attitude 鍐呯湡姝ｆ墽琛岋紝姝ゅ鎵嶈兘娓呴櫎鎸傝捣鏍囧織銆?*/
    if (orientation_filter.ready && attitude_zero_pending)
        attitude_zero_pending = false;

    /* refresh 璇锋眰锛堝 UI 鍒囬〉锛夊彲绔嬪嵆鍒锋柊鎱㈤€熻澶囷紝涓嶅繀骞茬瓑 500 ms 鍛ㄦ湡銆?*/
    if (refresh ||
        ((now - previous_slow_tick) >= rt_tick_from_millisecond(BOARD_SERVICE_SLOW_PERIOD_MS)))
    {
        previous_slow_tick = now;
        update_slow_devices(snapshot);
    }
    update_rw007(snapshot, rw007_reset);
    if (led_next)
    {
        snapshot->led_ring_mode = (uint8_t)led_ring_next();
        rt_kprintf("[LED] Active ring mode: %u (0=off 1=center 2=inner 3=outer)\n",
                   snapshot->led_ring_mode);
    }
    update_gpio_snapshot(snapshot);
    ++snapshot->sequence;
}

/**
 * @brief 鏈嶅姟寤鸿鐨勪换鍔″惊鐜懆鏈熴€? * @retval 鍛ㄦ湡姣鏁帮紝渚?rt_thread_mdelay 浣跨敤銆? */
uint32_t board_service_period_ms(void)
{
    return BOARD_SERVICE_FAST_PERIOD_MS;
}





