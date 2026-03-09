#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ws2812.h"
#include "switch.h"

volatile uint8_t should_exit; // for graceful shutdown

void app_main(void)
{
    xTaskCreatePinnedToCore(
        ws2812_task,
        "ws2812_task",
        4096,
        NULL,
        5,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        switch_task,
        "switch_task",
        2048,
        NULL,
        5,
        NULL,
        1
    );

    rgb_color_t on = {150, 100, 0};   // Red
    rgb_color_t off_standby = {0, 0, 10}; // All off (standby)

    ws2812_led_chain_t chain = WS2812_ALL_OFF;
    chain.colors[0] = off_standby;
    chain.active[0] = 1;

    while(1){

        ws2812_set_led_chain(chain); // Set first LED to standby color
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds

        ws2812_set_all_color(on); // Set color to red
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds

    }

    
}