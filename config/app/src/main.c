#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/endpoints.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void main(void) {
    LOG_INF("=== Joystick Mouse Firmware ===");
    LOG_INF("Board: nRF52840");
    LOG_INF("Features: USB HID Mouse with Joystick");
    LOG_INF("Built: %s %s", __DATE__, __TIME__);
    
    // 等待USB枚举
    k_msleep(3000);
    
    LOG_INF("Joystick mouse ready!");
    LOG_INF("Move the joystick to control mouse cursor");
    
    // 主循环，保持系统运行
    while (1) {
        // 定期发送空报告，保持USB连接活跃
        zmk_endpoints_send_mouse_report();
        k_msleep(1000);
    }
}