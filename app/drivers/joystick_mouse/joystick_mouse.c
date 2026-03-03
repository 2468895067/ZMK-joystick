#include "joystick_mouse.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(joystick, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/sys/util.h>

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

// 全局配置
static struct joystick_config config = {
    .deadzone = 100,
    .sensitivity = 5,
    .polling_interval_ms = 20,
    .invert_y = false,
    .always_active = true
};

// ADC配置
static struct adc_dt_spec x_axis = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static struct adc_dt_spec y_axis = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

// 校准值
static int16_t x_center = 2048;
static int16_t y_center = 2048;
static bool calibrated = false;

static K_THREAD_STACK_DEFINE(joystick_stack, JOYSTICK_STACK_SIZE);
static struct k_thread joystick_thread;
static k_tid_t joystick_tid;
static bool joystick_running = true;
static struct k_mutex adc_mutex;

// 读取ADC原始值
static int read_adc_raw(const struct adc_dt_spec *spec, int16_t *result) {
    int ret;
    
    if (!device_is_ready(adc_dev)) {
        return -ENODEV;
    }
    
    ret = adc_read(adc_dev, &spec->channel_cfg);
    if (ret < 0) {
        return ret;
    }
    
    *result = (int16_t)spec->raw_value;
    return 0;
}

// 自动校准
static void calibrate_joystick(void) {
    int16_t x_val, y_val;
    int32_t x_sum = 0, y_sum = 0;
    int samples = 10;
    
    LOG_INF("Calibrating joystick...");
    
    for (int i = 0; i < samples; i++) {
        if (read_adc_raw(&x_axis, &x_val) == 0 && 
            read_adc_raw(&y_axis, &y_val) == 0) {
            x_sum += x_val;
            y_sum += y_val;
        }
        k_msleep(10);
    }
    
    if (samples > 0) {
        x_center = x_sum / samples;
        y_center = y_sum / samples;
        calibrated = true;
        LOG_INF("Calibration complete: X=%d, Y=%d", x_center, y_center);
    }
}

// 应用死区和映射
static int8_t process_axis(int16_t raw_value, int16_t center) {
    int16_t delta = raw_value - center;
    
    // 应用死区
    if (delta > -config.deadzone && delta < config.deadzone) {
        return 0;
    }
    
    // 归一化到[-127, 127]并应用灵敏度
    int8_t mapped = (int8_t)CLAMP(
        delta * config.sensitivity / (ADC_MAX_VALUE / 2),
        -127, 127
    );
    
    return mapped;
}

// 主工作线程
static void joystick_work_thread(void *p1, void *p2, void *p3) {
    int16_t x_raw, y_raw;
    int8_t dx, dy;
    
    // 等待系统稳定
    k_msleep(1000);
    
    // 初始化ADC
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return;
    }
    
    adc_channel_setup(adc_dev, &x_axis.channel_cfg);
    adc_channel_setup(adc_dev, &y_axis.channel_cfg);
    
    // 校准
    calibrate_joystick();
    
    LOG_INF("Joystick mouse driver started (always active)");
    
    while (1) {
        if (joystick_running && calibrated) {
            if (read_adc_raw(&x_axis, &x_raw) == 0 &&
                read_adc_raw(&y_axis, &y_raw) == 0) {
                
                dx = process_axis(x_raw, x_center);
                dy = process_axis(y_raw, y_center);
                
                if (config.invert_y) {
                    dy = -dy;
                }
                
                // 发送鼠标移动
                if (dx != 0 || dy != 0) {
                    zmk_hid_mouse_movement_set(dx, dy);
                    zmk_hid_mouse_movement_send();
                    zmk_endpoints_send_mouse_report();
                }
            }
        }
        
        k_msleep(config.polling_interval_ms);
    }
}

// 启用/禁用摇杆
void joystick_set_enabled(bool enabled) {
    joystick_running = enabled;
    LOG_INF("Joystick %s", enabled ? "enabled" : "disabled");
}

bool joystick_is_enabled(void) {
    return joystick_running;
}

// 更新配置
void joystick_update_config(struct joystick_config *new_config) {
    k_mutex_lock(&adc_mutex, K_FOREVER);
    memcpy(&config, new_config, sizeof(config));
    k_mutex_unlock(&adc_mutex);
    LOG_INF("Joystick config updated");
}

// 初始化函数
static int joystick_init(const struct device *dev) {
    ARG_UNUSED(dev);
    
    k_mutex_init(&adc_mutex);
    
    joystick_tid = k_thread_create(
        &joystick_thread,
        joystick_stack,
        JOYSTICK_STACK_SIZE,
        joystick_work_thread,
        NULL, NULL, NULL,
        JOYSTICK_PRIORITY,
        0,
        K_NO_WAIT
    );
    
    k_thread_name_set(&joystick_thread, "joystick_mouse");
    
    return 0;
}

SYS_INIT(joystick_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);