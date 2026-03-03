cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(joystick_mouse)

# 包含自定义驱动
add_subdirectory_ifdef(CONFIG_JOYSTICK_DRIVER drivers/joystick)

# 主应用源文件
target_sources(app PRIVATE app/src/main.c)

# 设备树绑定
list(APPEND DTC_OVERLAY_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/board.overlay
    ${CMAKE_CURRENT_SOURCE_DIR}/joystick.overlay
    ${CMAKE_CURRENT_SOURCE_DIR}/keymap.dtsi
)