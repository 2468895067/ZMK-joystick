#pragma once

#include <zmk/behavior.h>
#include <drivers/joystick_mouse/joystick_mouse.h>

int zmk_behavior_joystick_mouse_always(const zmk_event_t *eh);
int zmk_behavior_joystick_toggle(const zmk_event_t *eh);