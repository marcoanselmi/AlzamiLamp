#pragma once
#include <stdint.h>
#include "ws2812.h"

/**
 * Command types understood by the lamp.
 * Include this header in any transport module (UDP, MQTT, HTTP…)
 * and in the lamp worker task.
 */
typedef enum {
    LAMP_CMD_NONE = 0,        // returned on timeout / empty queue
    LAMP_CMD_ON,
    LAMP_CMD_OFF,
    LAMP_CMD_SET_ON_COLOR,    // payload: r, g, b (0–255 each)
    LAMP_CMD_SET_OFF_COLOR,   // payload: r, g, b (0–255 each)
} lamp_cmd_type_t;

typedef struct {
    lamp_cmd_type_t type;
    rgb_color_t color;        // used for SET_ON_COLOR and SET_OFF_COLOR
} lamp_cmd_t;