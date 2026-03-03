#include "joystick.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>

LOG_MODULE_REGISTER(joystick, LOG_LEVEL_INF);

// 默认配置
static struct joystick_config config = {
    .x_channel = 0,
    .y_channel = 1,
    .deadzone = 100,
    .sensitivity = 5,
    .poll_interval = 20,
    .invert_y = false
};

// ADC设备
static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

// ADC通道配置
static struct adc_channel_cfg x_ch_cfg = {
    .gain = ADC_GAIN_1_5,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
    .channel_id = 0,
    .input_positive = 0
};

static struct adc_channel_cfg y_ch_cfg = {
    .gain = ADC_GAIN_1_5,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
    .channel_id = 1,
    .input_positive = 1
};

static K_THREAD_STACK_DEFINE(joystick_stack, 1024);
static struct k_thread joystick_thread;
static struct k_sem joystick_sem;
static bool joystick_active = true;
static bool initialized = false;
static int16_t center_x = 2048;
static int16_t center_y = 2048;

// 工作线程
static void joystick_thread_func(void *p1, void *p2, void *p3) {
    int16_t x_val, y_val;
    int8_t dx, dy;
    
    LOG_INF("Joystick thread started");
    
    while (1) {
        k_sem_take(&joystick_sem, K_FOREVER);
        
        if (!joystick_active || !initialized) {
            k_msleep(config.poll_interval);
            k_sem_give(&joystick_sem);
            continue;
        }
        
        // 读取ADC值
        if (adc_read(adc_dev, &x_ch_cfg) >= 0 && 
            adc_read(adc_dev, &y_ch_cfg) >= 0) {
            
            x_val = (int16_t)x_ch_cfg.raw_value;
            y_val = (int16_t)y_ch_cfg.raw_value;
            
            // 计算偏移
            dx = (int8_t)((x_val - center_x) * config.sensitivity / 2048);
            dy = (int8_t)((y_val - center_y) * config.sensitivity / 2048);
            
            // 应用死区
            if (dx > -config.deadzone/20 && dx < config.deadzone/20) dx = 0;
            if (dy > -config.deadzone/20 && dy < config.deadzone/20) dy = 0;
            
            // 反转Y轴
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
        
        k_msleep(config.poll_interval);
        k_sem_give(&joystick_sem);
    }
}

// 初始化ADC
static int init_adc(void) {
    int ret;
    
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }
    
    // 配置X轴通道
    x_ch_cfg.input_positive = config.x_channel;
    ret = adc_channel_setup(adc_dev, &x_ch_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to setup X channel: %d", ret);
        return ret;
    }
    
    // 配置Y轴通道
    y_ch_cfg.input_positive = config.y_channel;
    ret = adc_channel_setup(adc_dev, &y_ch_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to setup Y channel: %d", ret);
        return ret;
    }
    
    LOG_INF("ADC initialized: X=AIN%d, Y=AIN%d", config.x_channel, config.y_channel);
    return 0;
}

// 校准
static void calibrate(void) {
    int16_t x_sum = 0, y_sum = 0;
    const int samples = 20;
    
    LOG_INF("Calibrating joystick...");
    
    for (int i = 0; i < samples; i++) {
        if (adc_read(adc_dev, &x_ch_cfg) >= 0 && 
            adc_read(adc_dev, &y_ch_cfg) >= 0) {
            x_sum += (int16_t)x_ch_cfg.raw_value;
            y_sum += (int16_t)y_ch_cfg.raw_value;
        }
        k_msleep(5);
    }
    
    center_x = x_sum / samples;
    center_y = y_sum / samples;
    
    LOG_INF("Calibration complete: X=%d, Y=%d", center_x, center_y);
}

// 启动摇杆
void joystick_start(void) {
    if (!initialized) {
        if (init_adc() == 0) {
            calibrate();
            initialized = true;
            joystick_active = true;
            LOG_INF("Joystick started");
        }
    }
}

// 停止摇杆
void joystick_stop(void) {
    joystick_active = false;
    LOG_INF("Joystick stopped");
}

// 立即校准
void joystick_calibrate_now(void) {
    if (initialized) {
        calibrate();
    }
}

// 驱动初始化
static int joystick_init(const struct device *dev) {
    ARG_UNUSED(dev);
    
    k_sem_init(&joystick_sem, 1, 1);
    
    k_thread_create(&joystick_thread,
                   joystick_stack,
                   K_THREAD_STACK_SIZEOF(joystick_stack),
                   joystick_thread_func,
                   NULL, NULL, NULL,
                   K_PRIO_PREEMPT(6),
                   0,
                   K_MSEC(100));
    
    k_thread_name_set(&joystick_thread, "joystick_mouse");
    
    // 延迟启动，等待系统稳定
    k_work_submit(&(struct k_work_delayable){
        .work = {
            .handler = (k_work_handler_t)joystick_start,
        },
        .timeout = K_MSEC(2000)
    });
    
    LOG_INF("Joystick driver initialized");
    return 0;
}

SYS_INIT(joystick_init, APPLICATION, 99);