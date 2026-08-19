/**
 * @file config.h
 * @brief Centralized configuration constants for thread parameters, sampling periods, and Kalman filter coefficients.
 *
 * This file serves as the single source of truth (SSOT) for all tunable parameters,
 * allowing maintainers to see system behavior and performance characteristics at a glance
 * without needing to jump between multiple driver/service files.
 *
 * Categories:
 * - RT_THREAD_* : RT-Thread task configuration (priority, stack size, time slice)
 * - BOARD_SERVICE_* : Board-level service sampling periods
 * - KALMAN_* : Environment sensor one-dimensional Kalman filter parameters
 * - IMU_* : Inertial measurement unit configuration constants
 * - IO_* : GPIO and communication bus pin mappings
 */
#ifndef BSP_CONFIG_H__
#define BSP_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RT-Thread Task Configuration
 * ========================================================================== */

/**
 * @brief GUI thread priority (smaller number = higher priority).
 * Set to 18, slightly higher than board thread(19), to ensure keypad input
 * receives timely UI response.
 */
#define RT_THREAD_GUI_PRIORITY                18U
#define RT_THREAD_RT_THREAD_GUI_PRIORITY      18U

/**
 * @brief Board service thread priority.
 * Set to 19, lower than GUI(18), higher than FinSH default(20).
 */
#define RT_THREAD_BOARD_PRIORITY              19U
#define RT_THREAD_RT_THREAD_BOARD_PRIORITY    19U

/**
 * @brief Application thread time slice (milliseconds).
 * GUI and board threads share the same time slice for fair switching
 * when preempted by higher priority threads.
 */
#define RT_THREAD_APP_TIME_SLICE              10U
#define RT_THREAD_RT_THREAD_APP_TIME_SLICE    10U

/**
 * @brief GUI thread stack size (bytes).
 * Set to 6144, twice that of board thread, because LVGL draw call chains are deeper.
 */
#define RT_THREAD_GUI_STACK_SIZE              6144U
#define RT_THREAD_RT_THREAD_GUI_STACK_SIZE    6144U

/**
 * @brief Board service thread stack size (bytes).
 * Set to 3072, sufficient for driver calls, Kalman filtering, and quaternion math.
 */
#define RT_THREAD_BOARD_STACK_SIZE            3072U
#define RT_THREAD_RT_THREAD_BOARD_STACK_SIZE  3072U

/* ============================================================================
 * Board-Level Service Sampling Periods
 * ========================================================================== */

/**
 * @brief Fast-cycle sampling period (milliseconds).
 * 20 ms ensures IMU attitude integration step size dt is small enough for high precision;
 * other fast devices (AHT21 read, ICM20608 read) also complete within this cycle.
 */
#define BOARD_SERVICE_FAST_PERIOD_MS 20U

/**
 * @brief Slow-cycle sampling period (milliseconds).
 * 500 ms for low-frequency devices: light sensor(AP3216C), storage detection(W25Q/SD),
 * RW007 status query. These devices don't need high-frequency updates; low-frequency
 * polling reduces I2C bus contention.
 */
#define BOARD_SERVICE_SLOW_PERIOD_MS 500U

/* ============================================================================
 * Environment Sensor Kalman Filter Parameters (Scalar)
 * ========================================================================== */

/**
 * @brief Temperature Kalman filter process noise(Q) and measurement noise(R).
 * Q=0.02, R=0.30:
 *   - Smaller Q trusts the prediction model, smoother filter response
 *   - Larger R trusts measurements, responsive to jumps without excessive noise
 * Tuning hints: if filter lags excessively, increase Q; if noise remains visible, increase R.
 */
#define KALMAN_TEMPERATURE_Q         0.02f
#define KALMAN_TEMPERATURE_R         0.30f

/**
 * @brief Humidity Kalman filter process noise(Q) and measurement noise(R).
 * Q=0.05, R=0.80:
 *   - Q slightly larger than temperature (humidity changes typically slower)
 *   - R larger (humidity measurement noise typically higher than temperature)
 */
#define KALMAN_HUMIDITY_Q            0.05f
#define KALMAN_HUMIDITY_R            0.80f

/* ============================================================================
 * IMU Attitude Fusion Configuration
 * ========================================================================== */

/**
 * @brief IMU calibration required sample count.
 * 300 samples @ 50 Hz = 6 seconds; used for gyroscope bias and temperature coefficient learning.
 * Keep board still for the first 6 seconds after power-on; safe to move after calibration completes.
 */
#define ATTITUDE_CALIBRATION_SAMPLE_COUNT 300U

/* ============================================================================
 * GPIO and Communication Bus Pin Mappings
 * ========================================================================== */

/**
 * @brief AHT21 temperature/humidity sensor software I2C bus (bus 1).
 * Uses GPIOE PIN0(SDA) and PIN1(SCL); this bus is dedicated, avoiding contention with IMU/light sensor.
 */
#define IO_AHT21_I2C_PORT            GPIOE
#define IO_AHT21_I2C_SDA_PIN         GPIO_PIN_0
#define IO_AHT21_I2C_SCL_PIN         GPIO_PIN_1

/**
 * @brief ICM20608(IMU) and AP3216C(light sensor) shared software I2C bus (bus 2).
 * Uses GPIOF PIN1(SDA) and PIN0(SCL); two devices with different I2C addresses can coexist.
 */
#define IO_SENSOR_I2C_PORT           GPIOF
#define IO_SENSOR_I2C_SDA_PIN        GPIO_PIN_1
#define IO_SENSOR_I2C_SCL_PIN        GPIO_PIN_0

/**
 * @brief SPI2 bus interface (for W25Q SPI Flash and optional RW007 WiFi module).
 * Hardware SPI2 configuration is in Core/Src/stm32f4xx_hal_msp.c; this is a reference marker.
 */
#define IO_SPI2_INSTANCE             SPI2

/**
 * @brief LCD backlight GPIO (PF9, high level = on).
 */
#define IO_LCD_BACKLIGHT_PORT        GPIOF
#define IO_LCD_BACKLIGHT_PIN         GPIO_PIN_9

/**
 * @brief Red LED GPIO (PF5, used as system heartbeat indicator).
 * GUI thread toggles once per 500 ms; should see ~1 Hz blink rate during operation.
 */
#define IO_LED_RED_PORT              GPIOF
#define IO_LED_RED_PIN               GPIO_PIN_5

/**
 * @brief SD card insertion detection GPIO (PF3, low level = card inserted).
 */
#define IO_SD_DETECT_PORT            GPIOF
#define IO_SD_DETECT_PIN             GPIO_PIN_3

/**
 * @brief RW007 WiFi module reset GPIO (high level = reset active).
 */
#define IO_RW007_RST_PORT            GPIOC
#define IO_RW007_RST_PIN             GPIO_PIN_3

/**
 * @brief RW007 WiFi module interrupt/busy indicator GPIO (low level on interrupt).
 */
#define IO_RW007_INT_PORT            GPIOC
#define IO_RW007_INT_PIN             GPIO_PIN_2

#ifdef __cplusplus
}
#endif

#endif /* BSP_CONFIG_H__ */
