#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lamp_cmd.h"

#define LAMP_UDP_PORT 4210

/**
 * @brief  Create the command queue and start the UDP listener task.
 *
 * Blocks on recvfrom(), parses incoming JSON datagrams, enqueues
 * lamp_cmd_t values, and sends a JSON ACK back to the sender.
 *
 * Call AFTER WiFi is connected.
 */
void udp_start(void);

/**
 * @brief  Block until a command arrives or timeout expires.
 *
 *   while (1) {
 *       lamp_cmd_t cmd = udp_get_command(portMAX_DELAY);
 *       if (cmd.type != LAMP_CMD_NONE) { ... }
 *   }
 *
 * @param  timeout_ms  Wait in ms. portMAX_DELAY blocks forever.
 * @return lamp_cmd_t  Next command, or {.type = LAMP_CMD_NONE} on timeout.
 */
lamp_cmd_t udp_get_command(TickType_t timeout_ms);