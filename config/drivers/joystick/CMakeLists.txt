zephyr_library()

zephyr_library_sources(joystick.c)

zephyr_library_include_directories(.)

zephyr_library_compile_definitions(
    JOYSTICK_DRIVER=1
)