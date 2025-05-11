#pragma once

#include "pico/types.h"

typedef struct JoystickInfo {
    char* direction;
    uint max_value;
    uint x_raw;
    uint y_raw;
    float x_normalized;
    float y_normalized;
} JoystickInfo;

JoystickInfo joystick_get_info();
