#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stddef.h>
#include <stdbool.h>

// ─── Command types ────────────────────────────────────────────────────────────

typedef enum {
    LAMP_CMD_NONE = 0,     // returned by mqtt_get_command() on timeout
    LAMP_CMD_ON,
    LAMP_CMD_OFF,
    LAMP_CMD_BRIGHTNESS,   // payload: brightness (0–255)
    LAMP_CMD_COLOR,        // payload: r, g, b (0–255 each)
} lamp_cmd_type_t;

typedef struct {
    lamp_cmd_type_t type;
    uint8_t         brightness;
    uint8_t         r, g, b;
} lamp_cmd_t;

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * @brief  Initialize the internal command queue and start the MQTT client.
 *
 * Must be called AFTER WiFi is connected (WIFI_CONNECTED_BIT set).
 * The queue is created internally — no handle needs to be passed in.
 * esp_mqtt_client_start() spawns the library's own task internally.
 */
void mqtt_start(void);

/**
 * @brief  Block until a command arrives or timeout expires.
 *
 * Intended to be called from lamp_worker_task in a loop:
 *
 *   while (1) {
 *       lamp_cmd_t cmd = mqtt_get_command(portMAX_DELAY);
 *       if (cmd.type != LAMP_CMD_NONE) { ... }
 *   }
 *
 * @param  timeout_ms  How long to wait in ms. Pass portMAX_DELAY to block forever.
 * @return lamp_cmd_t  The next command, or {.type = LAMP_CMD_NONE} on timeout.
 */
lamp_cmd_t mqtt_get_command(TickType_t timeout_ms);

/**
 * @brief  Publish a status message back to the broker.
 *
 * Topic:   lamp/status
 * Payload: JSON string, e.g. {"state":"on","brightness":128}
 *
 * Safe to call from any task.
 */
void mqtt_publish_status(const char *json_payload);