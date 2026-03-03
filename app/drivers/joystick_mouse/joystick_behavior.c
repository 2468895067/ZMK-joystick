#include "joystick_behavior.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(joystick_behavior, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

// 始终激活的摇杆鼠标行为
int zmk_behavior_joystick_mouse_always(const zmk_event_t *eh) {
    // 这是一个虚拟行为，只用于确保摇杆驱动被包含在固件中
    // 实际的控制在驱动线程中持续运行
    return ZMK_BEHAVIOR_OPAQUE;
}

// 摇杆开关行为
int zmk_behavior_joystick_toggle(const zmk_event_t *eh) {
    if (as_zmk_position_state_changed(eh)) {
        const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
        
        if (ev->state) {  // 按键按下
            joystick_set_enabled(!joystick_is_enabled());
        }
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

// 行为定义
static const struct zmk_behavior_descriptor behaviors[] = {
    BEHAVIOR_DT_DESCRIPTOR(DT_NODELABEL(js_mouse_always), zmk_behavior_joystick_mouse_always),
    BEHAVIOR_DT_DESCRIPTOR(DT_NODELABEL(js_toggle), zmk_behavior_joystick_toggle),
};

static int joystick_behavior_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

SYS_INIT(joystick_behavior_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);