#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zmk/hid.h>

#define JOYSTICK_STACK_SIZE 512
#define JOYSTICK_PRIORITY 5

#define ADC_RESOLUTION 12
#define ADC_REF_VOLTAGE_MV 3600
#define ADC_MAX_VALUE ((1 << ADC_RESOLUTION) - 1)

#define DEADZONE 100
#define MOUSE_SPEED 5

struct joystick_config {
    struct adc_dt_spec x_axis;
    struct adc_dt_spec y_axis;
    uint16_t center_threshold;
    uint8_t polling_interval_ms;
};