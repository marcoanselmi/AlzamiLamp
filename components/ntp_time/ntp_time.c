#include "esp_log.h"
#include "esp_sntp.h"
//#include "esp_netif.h"
#include "esp_wifi.h"
#include "ntp_time.h"

#include "time.h"

void sync_time() {
    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 10) {
        ESP_LOGI("NTP", "Waiting for sync... (%d)", retry);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        retry++;
    }

    if (retry == 10) {
        ESP_LOGE("NTP", "Failed to synchronize time");
        return;
    } else {
        ESP_LOGI("NTP", "Time synchronized successfully");
    }

    // Set your timezone (Italy)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    return;
}

uint8_t get_time() {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    uint8_t hours = timeinfo.tm_hour;
    //uint8_t minutes = timeinfo.tm_min;

    return hours;
}