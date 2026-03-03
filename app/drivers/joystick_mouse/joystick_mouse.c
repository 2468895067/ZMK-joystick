#include "joystick_mouse.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(joystick, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/mouse_tick.h>

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

// ADC配置
static struct adc_dt_spec x_axis = ADC_DT_SPEC_GET_BY_IDX(DT_NODELABEL(adc), 0);
static struct adc_dt_spec y_axis = ADC_DT_SPEC_GET_BY_IDX(DT_NODELABEL(adc), 1);

// 采样缓冲区
static int16_t x_buf, y_buf;
static int16_t last_dx, last_dy;

static K_THREAD_STACK_DEFINE(joystick_thread_stack, JOYSTICK_STACK_SIZE);
static struct k_thread joystick_thread;
static k_tid_t joystick_tid;
static bool joystick_active = false;

// 读取ADC值
static int read_adc(const struct adc_dt_spec *spec, int16_t *result) {
    int ret;
    
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }
    
    ret = adc_read(adc_dev, &spec->channel_cfg);
    if (ret != 0) {
        LOG_ERR("ADC read failed: %d", ret);
        return ret;
    }
    
    *result = (int16_t)(spec->raw_value * ADC_REF_VOLTAGE_MV / ADC_MAX_VALUE);
    return 0;
}

// 映射ADC值到鼠标移动
static int8_t map_to_mouse(int16_t adc_value) {
    int16_t centered = adc_value - (ADC_MAX_VALUE / 2);
    
    // 死区处理
    if (centered > -DEADZONE && centered < DEADZONE) {
        return 0;
    }
    
    // 线性映射
    int8_t mouse_value = (int8_t)(centered * MOUSE_SPEED / (ADC_MAX_VALUE / 2));
    
    // 限制范围
    if (mouse_value > 127) mouse_value = 127;
    if (mouse_value < -128) mouse_value = -128;
    
    return mouse_value;
}

// 主处理线程
static void joystick_thread_func(void *p1, void *p2, void *p3) {
    int ret;
    
    // 初始化ADC
    ret = adc_channel_setup(adc_dev, &x_axis.channel_cfg);
    if (ret != 0) {
        LOG_ERR("Failed to setup X axis ADC: %d", ret);
        return;
    }
    
    ret = adc_channel_setup(adc_dev, &y_axis.channel_cfg);
    if (ret != 0) {
        LOG_ERR("Failed to setup Y axis ADC: %d", ret);
        return;
    }
    
    while (1) {
        if (joystick_active) {
            // 读取X轴
            ret = read_adc(&x_axis, &x_buf);
            if (ret == 0) {
                int8_t dx = map_to_mouse(x_buf);
                
                // 读取Y轴
                ret = read_adc(&y_axis, &y_buf);
                if (ret == 0) {
                    int8_t dy = map_to_mouse(y_buf);
                    
                    // 如果有移动，发送鼠标报告
                    if (dx != 0 || dy != 0) {
                        zmk_hid_mouse_movement_set(dx, -dy); // Y轴反转
                        zmk_hid_mouse_movement_send();
                        last_dx = dx;
                        last_dy = dy;
                    }
                }
            }
        }
        
        k_msleep(CONFIG_JOYSTICK_POLLING_INTERVAL);
    }
}

// 事件处理器
static int joystick_event_listener(const zmk_event_t *eh) {
    if (as_zmk_mouse_tick(eh)) {
        joystick_active = true;
        return 0;
    }
    
    return -ENOTSUP;
}

ZMK_LISTENER(joystick, joystick_event_listener);
ZMK_SUBSCRIPTION(joystick, zmk_mouse_tick);

// 初始化函数
static int joystick_init(const struct device *dev) {
    ARG_UNUSED(dev);
    
    joystick_tid = k_thread_create(&joystick_thread,
                                   joystick_thread_stack,
                                   JOYSTICK_STACK_SIZE,
                                   joystick_thread_func,
                                   NULL, NULL, NULL,
                                   JOYSTICK_PRIORITY,
                                   0, K_NO_WAIT);
    
    k_thread_name_set(&joystick_thread, "joystick_thread");
    
    LOG_INF("Joystick mouse driver initialized");
    return 0;
}

SYS_INIT(joystick_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);