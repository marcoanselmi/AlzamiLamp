#include "wifi.h"
#include "lamp_settings.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

#define AP_SSID      "AlzamiLamp"
#define AP_PASSWORD  "AlzamiConfig"
#define AP_CHANNEL   1
#define MAX_RETRY    5

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static const char *TAG = "WIFI";

static EventGroupHandle_t s_wifi_event_group = NULL;
static int                s_retry_count      = 0;
static bool               s_connected        = false;

// ─── Event handler STA ────────────────────────────────────────────────────────

static void sta_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry_count < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "Riconnessione... (%d/%d)", s_retry_count, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connessione fallita dopo %d tentativi", MAX_RETRY);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_connected   = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ─── Modalità AP ──────────────────────────────────────────────────────────────

static void start_ap(void)
{
    ESP_LOGI(TAG, "Avvio in modalita AP: \"%s\" - \"%s\"", AP_SSID, AP_PASSWORD);
    ESP_LOGI(TAG, "Connettiti a \"%s\" e apri http://192.168.4.1", AP_SSID);

    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = AP_SSID,
            .ssid_len       = strlen(AP_SSID),
            .channel        = AP_CHANNEL,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
            .password       = AP_PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
}

// ─── Modalità STA ─────────────────────────────────────────────────────────────

static bool start_sta(const char *ssid, const char *password,
                      const char *ip_static)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // IP statico se configurato, altrimenti DHCP
    if (strlen(ip_static) > 0) {
        esp_netif_ip_info_t ip_info = {0};

        // Converte tutte le stringhe con esp_netif_str_to_ip4 — compatibile con tutte le versioni LwIP
        bool ip_ok = (esp_netif_str_to_ip4(ip_static,     &ip_info.ip)      == ESP_OK);
        bool nm_ok = (esp_netif_str_to_ip4("255.255.255.0", &ip_info.netmask) == ESP_OK);

        // Gateway: sostituisce l'ultimo ottetto con .1 (es. 192.168.1.111 → 192.168.1.1)
        // I byte sono in little-endian su ESP32: il primo ottetto è nel byte meno significativo
        bool gw_ok = false;
        if (ip_ok) {
            ip_info.gw.addr = (ip_info.ip.addr & 0x00FFFFFF) | 0x01000000;
            gw_ok = true;
        }

        if (ip_ok && nm_ok && gw_ok) {
            esp_netif_dhcpc_stop(netif);
            esp_netif_set_ip_info(netif, &ip_info);
            ESP_LOGI(TAG, "IP statico: %s", ip_static);
        } else {
            ESP_LOGW(TAG, "IP statico non valido ('%s') — uso DHCP", ip_static);
        }
    } else {
        ESP_LOGI(TAG, "Nessun IP statico configurato — uso DHCP");
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        sta_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        sta_event_handler, NULL, NULL);

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid,     ssid,     sizeof(sta_cfg.sta.ssid)     - 1);
    strncpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();

    // Aspetta connessione o fallimento
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connesso a \"%s\"", ssid);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        return true;
    }

    ESP_LOGE(TAG, "Impossibile connettersi a \"%s\"", ssid);
    return false;
}

// ─── API pubblica ─────────────────────────────────────────────────────────────

bool wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Leggi credenziali e IP da wifi_settings
    setting_value_t ssid, password, ip_static;
    wifi_settings_get(SETTING_KEY_SSID,      &ssid);
    wifi_settings_get(SETTING_KEY_PASSWORD,  &password);
    wifi_settings_get(SETTING_KEY_IP_STATIC, &ip_static);

    // SSID vuoto → vai direttamente in AP
    if (strlen(ssid.as_str) == 0) {
        ESP_LOGI(TAG, "Nessun SSID configurato → modalita AP");
        start_ap();
        return false;
    }

    // Tenta connessione STA
    ESP_LOGI(TAG, "Connessione a \"%s\"...", ssid.as_str);
    if (start_sta(ssid.as_str, password.as_str, ip_static.as_str)) {
        return true;
    }

    // Connessione fallita → AP come fallback
    ESP_LOGW(TAG, "Connessione fallita → modalita AP");
    start_ap();
    return false;
}

bool wifi_is_connected(void)
{
    return s_connected;
}

bool wifi_wait_for_connection(void)
{
    if (!s_wifi_event_group) {
        // Siamo in AP mode — nessun event group creato, non c'è niente da aspettare
        return false;
    }
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
    return s_connected;
}
