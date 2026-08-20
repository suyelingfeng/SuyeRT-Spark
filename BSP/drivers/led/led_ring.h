/**
 * @file led_ring.h
 * @brief Driver for the RT-Spark 19-pixel SK6805 circular RGB LED matrix.
 */
#ifndef LED_RING_H
#define LED_RING_H

#include <stdint.h>

#define LED_RING_PIXEL_COUNT 19U

/** Only one physical ring is lit at a time to limit current and visual noise. */
typedef enum
{
    LED_RING_OFF = 0,
    LED_RING_CENTER,
    LED_RING_INNER,
    LED_RING_OUTER,
    LED_RING_MODE_COUNT
} led_ring_mode_t;

/** Configure PA7/PF2 and send an all-off frame. */
void led_ring_init(void);

/** Display one exclusive ring. Invalid values are treated as OFF. */
void led_ring_set_mode(led_ring_mode_t mode);

/** Advance OFF -> CENTER -> INNER -> OUTER -> OFF and return the new mode. */
led_ring_mode_t led_ring_next(void);

/** Return the last mode sent to the LEDs. */
led_ring_mode_t led_ring_get_mode(void);

#endif /* LED_RING_H */
