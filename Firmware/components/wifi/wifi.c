#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

#define WIFI_SSID      "Vodafone-uaifai"
#define WIFI_PASSWORD  "Sdisalottino"
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY  5

static EventGroupHandle_t wifi_event_group;
static int retry_count = 0;

// Event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < MAX_RETRY) {
            esp_wifi_connect();
            retry_count++;
            ESP_LOGI("WIFI", "Retrying connection... (%d)", retry_count);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE("WIFI", "Failed to connect");
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();

    // Save the netif handle this time
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // ── Static IP ────────────────────────────────────────────────
    esp_netif_dhcpc_stop(netif);

    esp_netif_ip_info_t ip_info = {
        .ip      = { .addr = ESP_IP4TOADDR(192, 168, 1, 111) },
        .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) },
        .gw      = { .addr = ESP_IP4TOADDR(192, 168, 1,  1) },
    };
    esp_netif_set_ip_info(netif, &ip_info);
    // ─────────────────────────────────────────────────────────────

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("WIFI", "Connected successfully");
    } else {
        ESP_LOGE("WIFI", "Connection failed");
    }

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    return;
}

void wifi_wait_for_connection() {
    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
}