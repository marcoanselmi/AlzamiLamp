#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "stdbool.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "nvs_flash.h"

#include "ws2812.h"
#include "switch.h"
#include "main_logic.h"
#include "wifi.h"
#include "ntp_time.h"
#include "lamp_udp.h"
#include "lamp_mqtt.h"
#include "lamp_settings.h"
#include "lamp_cmd.h"
#include "lamp_http.h"

volatile uint8_t should_exit; // for graceful shutdown

void app_main(void)
{
    // Set light sleep mode
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 10,
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);

    // Initialize settings
    settings_init();

    // Initialize command queue (must be before any task that uses it)
    lamp_cmd_queue_init();
    xTaskCreate(
        ws2812_task,
        "ws2812_task",
        4096,
        NULL,
        5,
        NULL
    );

    // Stand switch task
    xTaskCreate(
        switch_task,
        "switch_task",
        2048,
        NULL,
        5,
        NULL
    );

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks initialize

    // Main logic task
    xTaskCreate(
        main_logic_task,
        "main_logic_task",
        6144,
        NULL,
        5,
        NULL
    );

    // WiFi and network
    bool is_sta = wifi_init();  // true = STA, false = AP

    http_server_start(!is_sta); // false = modalità Station (non AP)
    
    //sync_time();
    //uint8_t current_hour = get_time();
    //ESP_LOGI("MAIN", "Current hour: %d", current_hour);

    if (!is_sta) {
        ESP_LOGI("MAIN", "Modalità AP — servizi UDP e MQTT disabilitati");
        return;
    }

    ESP_LOGI("MAIN", "Modalità STA — avvio servizi UDP e MQTT se abilitati");
    udp_start(); // Init UDP listener (only in STA mode, as per settings)
    mqtt_start(); // Init MQTT client (only in STA mode, as per settings)
}