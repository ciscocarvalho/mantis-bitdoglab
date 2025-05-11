#include "../inc/joystick.h"

#include <math.h>
#include <stdlib.h>

#include "hardware/adc.h"

static char* get_direction(float x, float y) {
    if (fabsf(x) < 0.2f && fabsf(y) < 0.2f) {
        return "";
    }

    float ang = atan2f(y, x) * 180.f / (float) M_PI;

    if (ang < -157.5f) {
        return "oeste";
    } else if (ang < -112.5f) {
        return "sudoeste";
    } else if (ang < -67.5f) {
        return "sul";
    } else if (ang < -22.5f) {
        return "sudeste";
    } else if (ang < 22.5f) {
        return "leste";
    } else if (ang < 67.5f) {
        return "nordeste";
    } else if (ang < 112.5f) {
        return "norte";
    } else if (ang < 157.5f) {
        return "noroeste";
    } else {
        return "oeste";
    }
}

JoystickInfo joystick_get_info() {
    JoystickInfo info = {};

    adc_select_input(0);
    info.y_raw = adc_read();

    adc_select_input(1);
    info.x_raw = adc_read();

    info.max_value = (1 << 12) - 1;

    info.x_normalized = info.x_raw / (float) info.max_value;
    info.y_normalized = info.y_raw / (float) info.max_value;

    uint center = info.max_value / 2;

    uint x_raw_distance = (uint) abs((int) info.x_raw - (int) center);
    uint y_raw_distance = (uint) abs((int) info.y_raw - (int) center);

    uint raw_distance = sqrt(pow(x_raw_distance, 2) + pow(y_raw_distance, 2));
    float normalized_distance = raw_distance / (float) center;

    info.direction = "";

    int x_offset = (int) info.x_raw - (int) center;
    int y_offset = (int) info.y_raw - (int) center;

    if (normalized_distance > 0.5) {
        info.direction = get_direction(x_offset, y_offset);
    }

    return info;
}
