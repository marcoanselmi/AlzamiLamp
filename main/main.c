#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ws2812.h"
#include "switch.h"
#include "main_logic.h"
#include "wifi.h"
#include "ntp_time.h"
#include "lamp_mqtt.h"

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

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks initialize

    xTaskCreatePinnedToCore(
        main_logic_task,
        "main_logic_task",
        2048,
        NULL,
        5,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        wifi_init,
        "wifi_init",
        4096,
        NULL,
        5,
        NULL,
        0
    );

    wifi_wait_for_connection();
    
    //sync_time();
    //uint8_t current_hour = get_time();
    //ESP_LOGI("MAIN", "Current hour: %d", current_hour);

    mqtt_start(); // Pass command queue if needed

}