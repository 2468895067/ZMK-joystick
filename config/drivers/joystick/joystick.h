#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

// 摇杆配置结构
struct joystick_config {
	uint8_t x_adc_channel;
	uint8_t y_adc_channel;
	uint16_t deadzone;
	uint8_t sensitivity;
	uint16_t polling_interval_ms;
	bool invert_y;
	bool auto_calibration;
};

// API函数
void joystick_start(void);
void joystick_stop(void);
bool joystick_is_running(void);
void joystick_calibrate_now(void);
void joystick_update_config(const struct joystick_config *config);

#endif /* JOYSTICK_H */