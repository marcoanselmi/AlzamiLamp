#include "esp_err.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "math.h"

#include "main_logic.h"
#include "ws2812.h"
#include "switch.h"
#include "lamp_mqtt.h"

void main_logic_task(void *pvParameters)
{
    rgb_color_t on = {150, 100, 0};   // Red
    //rgb_color_t off_standby = {0, 20, 20}; // All off (standby)

    ws2812_led_chain_t off_chain = WS2812_ALL_OFF;
    off_chain.colors[0] = (rgb_color_t) {0, 30, 30};
    off_chain.active[0] = 1;
    off_chain.colors[1] = (rgb_color_t) {0, 20, 20};
    off_chain.active[1] = 1;

    ws2812_led_chain_t on_chain = WS2812_ALL_COLOR(on);

    ws2812_led_chain_t actual_chain = WS2812_ALL_OFF;
    ws2812_led_chain_t desired_chain = WS2812_ALL_OFF;

    TickType_t last_wake_time = xTaskGetTickCount();

    while(!should_exit) {

        // Check for switch events and update desired state
        uint8_t event = switch_get_event();
        if (event == 0) { // upright
            desired_chain = on_chain;
        } else if (event == 1) { // tilted
            desired_chain = off_chain;
        }

        lamp_cmd_t cmd;
        cmd = mqtt_get_command(0); // Non-blocking check for MQTT command
        if (cmd.type != 0) { // If a command was received
            ESP_LOGI("MAIN_LOGIC", "Received MQTT command: type=%d", cmd.type);
            switch (cmd.type) {
                case LAMP_CMD_ON:
                    desired_chain = on_chain;
                    break;
                case LAMP_CMD_OFF:
                    desired_chain = off_chain;
                    break;
                case LAMP_CMD_BRIGHTNESS:
                    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
                        desired_chain.colors[i].r = (desired_chain.colors[i].r * cmd.brightness);
                        desired_chain.colors[i].g = (desired_chain.colors[i].g * cmd.brightness);
                        desired_chain.colors[i].b = (desired_chain.colors[i].b * cmd.brightness);
                    }
                    break;
                case LAMP_CMD_COLOR:
                    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
                        desired_chain.colors[i].r = cmd.r;
                        desired_chain.colors[i].g = cmd.g;
                        desired_chain.colors[i].b = cmd.b;
                    }
                    break;
                default:
                    break;
            }
        }

        static int transition_step = 5;

        // Smoothly transition to desired state if different from actual
        if (memcmp(&actual_chain, &desired_chain, sizeof(ws2812_led_chain_t)) != 0) {
            // Compute the difference
            for (int i = 0; i < WS2812_NUM_LEDS; i++) {
                // Transition towards desired color
                if (desired_chain.active[i] == 0) {
                    // If desired is off, fade out
                    if (actual_chain.colors[i].r < transition_step) {
                        actual_chain.colors[i].r = 0;
                    } else {
                        actual_chain.colors[i].r -= transition_step;
                    }
                    if (actual_chain.colors[i].g < transition_step) {
                        actual_chain.colors[i].g = 0;
                    } else {
                        actual_chain.colors[i].g -= transition_step;
                    }
                    if (actual_chain.colors[i].b < transition_step) {
                        actual_chain.colors[i].b = 0;
                    } else {
                        actual_chain.colors[i].b -= transition_step;
                    }

                } else {
                    // If desired is on, fade in
                    if ( abs(desired_chain.colors[i].r - actual_chain.colors[i].r) < transition_step) {
                        actual_chain.colors[i].r = desired_chain.colors[i].r;
                    } else {
                        actual_chain.colors[i].r += transition_step * (desired_chain.colors[i].r > actual_chain.colors[i].r ? 1 : -1);
                    }
                    if ( abs(desired_chain.colors[i].g - actual_chain.colors[i].g) < transition_step) {
                        actual_chain.colors[i].g = desired_chain.colors[i].g;
                    } else {
                        actual_chain.colors[i].g += transition_step * (desired_chain.colors[i].g > actual_chain.colors[i].g ? 1 : -1);
                    }
                    if (abs(desired_chain.colors[i].b - actual_chain.colors[i].b) < transition_step) {
                        actual_chain.colors[i].b = desired_chain.colors[i].b;
                    } else {
                        actual_chain.colors[i].b += transition_step * (desired_chain.colors[i].b > actual_chain.colors[i].b ? 1 : -1);
                    }

                }
                actual_chain.active[i] = 1;
                if (actual_chain.colors[i].r < transition_step && actual_chain.colors[i].g < transition_step && actual_chain.colors[i].b < transition_step) {
                    actual_chain.active[i] = 0; // Consider it off when very dim
                }
                
            }
            ws2812_set_led_chain(actual_chain);
        }



        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(20));

    }

    vTaskDelete(NULL);
}