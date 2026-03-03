#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>

#define JOYSTICK_STACK_SIZE 1024
#define JOYSTICK_PRIORITY 5

#define ADC_RESOLUTION 12
#define ADC_MAX_VALUE ((1 << ADC_RESOLUTION) - 1)

struct joystick_config {
    int16_t deadzone;
    uint8_t sensitivity;
    uint8_t polling_interval_ms;
    bool invert_y;
    bool always_active;
};