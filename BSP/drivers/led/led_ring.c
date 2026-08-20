/**
 * @file led_ring.c
 * @brief Timing driver and physical ring mapping for the SK6805 LED matrix.
 *
 * Schematic mapping:
 *   PA7 -> 74LVC1G125 -> SK6805 DIN
 *   PF2 -> 74LVC1G125 /OE (low enables the data path)
 *
 * Pixel order follows the board layout: pixel 0 is the centre, pixels 1..6
 * form the inner ring and pixels 7..18 form the outer ring. SK6805 accepts an
 * 800 kbit/s GRB stream. Interrupts are masked only while the 19-pixel frame
 * is transmitted (about 0.6 ms), then restored before the reset-low period.
 */
#include "led_ring.h"
#include "config.h"
#include "main.h"
#include <rtthread.h>

/* Timings are expressed in CPU cycles and calculated at run time from HCLK. */
#define SK6805_BIT_NS       1250U
#define SK6805_T0H_NS        320U
#define SK6805_T1H_NS        640U
#define SK6805_RESET_US       80U

static led_ring_mode_t current_mode = LED_RING_OFF;

static uint32_t ns_to_cycles(uint32_t ns)
{
    uint32_t mhz = HAL_RCC_GetHCLKFreq() / 1000000U;
    return (mhz * ns + 999U) / 1000U;
}

static void dwt_start(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void wait_cycles(uint32_t started, uint32_t cycles)
{
    while ((uint32_t)(DWT->CYCCNT - started) < cycles)
    {
        __NOP();
    }
}

static inline void write_bit(uint8_t one, uint32_t bit_cycles,
                             uint32_t t0h_cycles, uint32_t t1h_cycles)
{
    uint32_t started = DWT->CYCCNT;
    IO_LED_MATRIX_DATA_PORT->BSRR = IO_LED_MATRIX_DATA_PIN;
    wait_cycles(started, one ? t1h_cycles : t0h_cycles);
    IO_LED_MATRIX_DATA_PORT->BSRR = (uint32_t)IO_LED_MATRIX_DATA_PIN << 16U;
    wait_cycles(started, bit_cycles);
}

static void write_byte(uint8_t value, uint32_t bit_cycles,
                       uint32_t t0h_cycles, uint32_t t1h_cycles)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
        write_bit((value & mask) != 0U, bit_cycles, t0h_cycles, t1h_cycles);
}

static void pixel_rgb(uint8_t index, led_ring_mode_t mode,
                      uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = 0U;
    *green = 0U;
    *blue = 0U;

    /* Brightness is deliberately limited to reduce 5 V rail transients. */
    if ((mode == LED_RING_CENTER) && (index == 0U))
    {
        *red = 12U; *green = 12U; *blue = 8U;
    }
    else if ((mode == LED_RING_INNER) && (index >= 1U) && (index <= 6U))
    {
        *green = 18U; *blue = 10U;
    }
    else if ((mode == LED_RING_OUTER) && (index >= 7U))
    {
        *green = 5U; *blue = 18U;
    }
}

static void send_frame(led_ring_mode_t mode)
{
    uint32_t bit_cycles = ns_to_cycles(SK6805_BIT_NS);
    uint32_t t0h_cycles = ns_to_cycles(SK6805_T0H_NS);
    uint32_t t1h_cycles = ns_to_cycles(SK6805_T1H_NS);
    rt_base_t level = rt_hw_interrupt_disable();

    for (uint8_t pixel = 0U; pixel < LED_RING_PIXEL_COUNT; ++pixel)
    {
        uint8_t red, green, blue;
        pixel_rgb(pixel, mode, &red, &green, &blue);
        write_byte(green, bit_cycles, t0h_cycles, t1h_cycles); /* GRB */
        write_byte(red,   bit_cycles, t0h_cycles, t1h_cycles);
        write_byte(blue,  bit_cycles, t0h_cycles, t1h_cycles);
    }
    IO_LED_MATRIX_DATA_PORT->BSRR = (uint32_t)IO_LED_MATRIX_DATA_PIN << 16U;
    rt_hw_interrupt_enable(level);

    /* A low interval latches the complete frame into all 19 LEDs. */
    {
        uint32_t started = DWT->CYCCNT;
        uint32_t reset_cycles = (HAL_RCC_GetHCLKFreq() / 1000000U) * SK6805_RESET_US;
        wait_cycles(started, reset_cycles);
    }
}

void led_ring_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    dwt_start();

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Pin = IO_LED_MATRIX_DATA_PIN;
    HAL_GPIO_Init(IO_LED_MATRIX_DATA_PORT, &gpio);
    gpio.Pin = IO_LED_MATRIX_ENABLE_PIN;
    HAL_GPIO_Init(IO_LED_MATRIX_ENABLE_PORT, &gpio);

    HAL_GPIO_WritePin(IO_LED_MATRIX_DATA_PORT, IO_LED_MATRIX_DATA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IO_LED_MATRIX_ENABLE_PORT, IO_LED_MATRIX_ENABLE_PIN, GPIO_PIN_RESET);
    current_mode = LED_RING_OFF;
    send_frame(current_mode);
}

void led_ring_set_mode(led_ring_mode_t mode)
{
    if (mode >= LED_RING_MODE_COUNT) mode = LED_RING_OFF;
    current_mode = mode;
    send_frame(mode);
}

led_ring_mode_t led_ring_next(void)
{
    led_ring_set_mode((led_ring_mode_t)((current_mode + 1U) % LED_RING_MODE_COUNT));
    return current_mode;
}

led_ring_mode_t led_ring_get_mode(void)
{
    return current_mode;
}
