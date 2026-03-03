#include "joystick.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>

LOG_MODULE_REGISTER(joystick, LOG_LEVEL_INF);

// 默认配置
static struct joystick_config config = {
	.x_adc_channel = 0,
	.y_adc_channel = 1,
	.deadzone = 100,
	.sensitivity = 5,
	.polling_interval_ms = 20,
	.invert_y = false,
	.auto_calibration = true
};

// 静态变量
static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static struct k_work_delayable joystick_work;
static bool joystick_enabled = true;
static bool calibrated = false;
static int16_t center_x = 2048;
static int16_t center_y = 2048;
static const int16_t adc_max = 4095;
static struct k_mutex config_mutex;
static bool initialized = false;

// ADC通道配置
static struct adc_channel_cfg ch0_cfg = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
	.channel_id = 0,
	.input_positive = 0
};

static struct adc_channel_cfg ch1_cfg = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
	.channel_id = 1,
	.input_positive = 1
};

// 读取ADC值
static int read_adc_value(uint8_t channel, int16_t *value) {
	int ret;
	
	if (!device_is_ready(adc_dev)) {
		return -ENODEV;
	}
	
	if (channel == 0) {
		ret = adc_read(adc_dev, &ch0_cfg);
		if (ret >= 0) {
			*value = (int16_t)ch0_cfg.raw_value;
		}
	} else {
		ret = adc_read(adc_dev, &ch1_cfg);
		if (ret >= 0) {
			*value = (int16_t)ch1_cfg.raw_value;
		}
	}
	
	return (ret >= 0) ? 0 : ret;
}

// 校准摇杆
static void calibrate(void) {
	int16_t x_sum = 0, y_sum = 0;
	const int samples = 10;
	int valid_samples = 0;
	
	LOG_INF("Calibrating joystick...");
	
	k_mutex_lock(&config_mutex, K_FOREVER);
	
	for (int i = 0; i < samples; i++) {
		int16_t x_val, y_val;
		
		if (read_adc_value(config.x_adc_channel, &x_val) == 0 &&
			read_adc_value(config.y_adc_channel, &y_val) == 0) {
			x_sum += x_val;
			y_sum += y_val;
			valid_samples++;
		}
		k_msleep(5);
	}
	
	if (valid_samples > 0) {
		center_x = x_sum / valid_samples;
		center_y = y_sum / valid_samples;
		calibrated = true;
		LOG_INF("Calibrated: X=%d, Y=%d", center_x, center_y);
	} else {
		LOG_WRN("Calibration failed");
	}
	
	k_mutex_unlock(&config_mutex);
}

// 处理摇杆移动
static void process_movement(int16_t x_raw, int16_t y_raw) {
	int16_t dx = x_raw - center_x;
	int16_t dy = y_raw - center_y;
	
	// 应用死区
	if (dx > -config.deadzone && dx < config.deadzone) dx = 0;
	if (dy > -config.deadzone && dy < config.deadzone) dy = 0;
	
	if (dx == 0 && dy == 0) {
		return;
	}
	
	// 计算鼠标移动
	int8_t mouse_x = (int8_t)(dx * config.sensitivity / (adc_max / 2));
	int8_t mouse_y = (int8_t)(dy * config.sensitivity / (adc_max / 2));
	
	// 限制范围
	mouse_x = CLAMP(mouse_x, -127, 127);
	mouse_y = CLAMP(mouse_y, -127, 127);
	
	// 反转Y轴
	if (config.invert_y) {
		mouse_y = -mouse_y;
	}
	
	// 发送鼠标移动
	zmk_hid_mouse_movement_set(mouse_x, mouse_y);
	zmk_hid_mouse_movement_send();
	zmk_endpoints_send_mouse_report();
	
	// 调试输出
	static uint32_t count = 0;
	if ((count++ % 50) == 0) {
		LOG_DBG("Mouse: dx=%d, dy=%d", mouse_x, mouse_y);
	}
}

// 工作队列处理函数
static void joystick_work_handler(struct k_work *work) {
	ARG_UNUSED(work);
	
	if (!joystick_enabled || !calibrated || !initialized) {
		k_work_reschedule(&joystick_work, K_MSEC(config.polling_interval_ms));
		return;
	}
	
	int16_t x_val, y_val;
	
	if (read_adc_value(config.x_adc_channel, &x_val) == 0 &&
		read_adc_value(config.y_adc_channel, &y_val) == 0) {
		process_movement(x_val, y_val);
	}
	
	// 重新调度
	k_work_reschedule(&joystick_work, K_MSEC(config.polling_interval_ms));
}

// 初始化ADC
static int init_adc(void) {
	int ret;
	
	if (!device_is_ready(adc_dev)) {
		LOG_ERR("ADC device not ready");
		return -ENODEV;
	}
	
	// 设置X轴通道
	ch0_cfg.input_positive = config.x_adc_channel;
	ret = adc_channel_setup(adc_dev, &ch0_cfg);
	if (ret != 0) {
		LOG_ERR("Failed to setup ADC channel 0: %d", ret);
		return ret;
	}
	
	// 设置Y轴通道
	ch1_cfg.input_positive = config.y_adc_channel;
	ret = adc_channel_setup(adc_dev, &ch1_cfg);
	if (ret != 0) {
		LOG_ERR("Failed to setup ADC channel 1: %d", ret);
		return ret;
	}
	
	LOG_INF("ADC initialized: X=AIN%d, Y=AIN%d", config.x_adc_channel, config.y_adc_channel);
	return 0;
}

// API函数实现
void joystick_start(void) {
	if (!initialized) {
		if (init_adc() == 0) {
			if (config.auto_calibration) {
				calibrate();
			} else {
				calibrated = true;
			}
			initialized = true;
			joystick_enabled = true;
			
			// 启动工作队列
			k_work_schedule(&joystick_work, K_MSEC(100));
			LOG_INF("Joystick started");
		}
	} else if (!joystick_enabled) {
		joystick_enabled = true;
		k_work_schedule(&joystick_work, K_MSEC(100));
		LOG_INF("Joystick resumed");
	}
}

void joystick_stop(void) {
	joystick_enabled = false;
	LOG_INF("Joystick stopped");
}

bool joystick_is_running(void) {
	return joystick_enabled && initialized;
}

void joystick_calibrate_now(void) {
	if (initialized) {
		calibrate();
	}
}

void joystick_update_config(const struct joystick_config *new_config) {
	k_mutex_lock(&config_mutex, K_FOREVER);
	memcpy(&config, new_config, sizeof(config));
	k_mutex_unlock(&config_mutex);
	LOG_INF("Joystick config updated");
}

// 初始化函数
static int joystick_init(const struct device *dev) {
	ARG_UNUSED(dev);
	
	LOG_INF("Initializing joystick driver...");
	
	// 初始化互斥锁
	k_mutex_init(&config_mutex);
	
	// 初始化工作队列
	k_work_init_delayable(&joystick_work, joystick_work_handler);
	
	// 延迟启动摇杆，等待系统稳定
	k_work_schedule(&joystick_work, K_MSEC(3000));
	
	LOG_INF("Joystick driver initialized");
	return 0;
}

SYS_INIT(joystick_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);