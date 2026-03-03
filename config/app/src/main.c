#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/endpoints.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void main(void) {
	LOG_INF("=========================================");
	LOG_INF("  Joystick Mouse Firmware for Nice!Nano  ");
	LOG_INF("=========================================");
	LOG_INF("Features:");
	LOG_INF("  - USB HID Mouse");
	LOG_INF("  - Bluetooth HID Mouse");
	LOG_INF("  - Joystick control (X/Y axis)");
	LOG_INF("  - Battery monitoring");
	LOG_INF("");
	LOG_INF("Waiting for USB enumeration...");
	
	// 等待系统稳定
	k_msleep(3000);
	
	LOG_INF("System ready!");
	LOG_INF("Joystick mouse is now active.");
	LOG_INF("Move joystick to control mouse cursor.");
	LOG_INF("");
	LOG_INF("Connect via:");
	LOG_INF("  - USB: Plug in USB cable");
	LOG_INF("  - Bluetooth: 'Joystick Mouse'");
	
	uint32_t counter = 0;
	
	while (1) {
		k_msleep(1000);
		
		// 定期打印状态
		if ((counter++ % 10) == 0) {
			LOG_INF("System running - Joystick active");
		}
		
		// 定期发送空报告，保持连接活跃
		zmk_endpoints_send_mouse_report();
	}
}