/**
 * @file gpio_monitor.h
 * @brief Read-only GPIOA..GPIOI register snapshot for UI and shell diagnosis.
 */
#ifndef GPIO_MONITOR_H
#define GPIO_MONITOR_H

#include <stdint.h>

#define GPIO_MONITOR_PORT_COUNT 9U

typedef struct
{
    uint16_t input;  /* IDR: actual logic level sampled at each pin. */
    uint16_t output; /* ODR: output latch value, meaningful for output pins. */
    uint32_t mode;   /* MODER: two bits per pin (input/output/AF/analog). */
} gpio_monitor_port_t;

/** Enable the AHB clocks needed to observe GPIOA..GPIOI registers. */
void gpio_monitor_init(void);

/** Copy one coherent software snapshot of GPIOA..GPIOI. */
void gpio_monitor_sample(gpio_monitor_port_t ports[GPIO_MONITOR_PORT_COUNT]);

#endif /* GPIO_MONITOR_H */
