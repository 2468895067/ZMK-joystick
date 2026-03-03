#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

// 配置结构
struct joystick_config {
    uint8_t x_channel;
    uint8_t y_channel;
    uint16_t deadzone;
    uint8_t sensitivity;
    uint16_t poll_interval;
    bool invert_y;
};

// API函数
void joystick_start(void);
void joystick_stop(void);
void joystick_calibrate_now(void);

#endif /* JOYSTICK_H */