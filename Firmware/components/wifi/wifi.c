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

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1
#define WIFI_DISCONNECTED_BIT   BIT2

static const char *TAG = "WIFI";

static EventGroupHandle_t s_wifi_event_group = NULL;
static int                s_retry_count      = 0;
static bool               s_connected        = false;
static bool               s_is_ap            = false;
static bool               s_is_sta           = false;

static esp_event_handler_instance_t s_sta_wifi_handler_instance;
static esp_event_handler_instance_t s_sta_ip_handler_instance;


static void stop_sta();
static bool start_sta(const char *ssid, const char *password, const char *ip_static);


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
            xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
            ESP_LOGE(TAG, "Connessione fallita dopo %d tentativi", MAX_RETRY);
            s_is_sta = false; // Flag per indicare che non siamo più in modalità STA
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

void wifi_start_ap_mode(void)
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

    s_is_ap = true;
}

void wifi_stop_ap_mode(void)
{
    esp_wifi_stop();
    s_is_ap = false;
    return;
}

// ─── Modalità STA ─────────────────────────────────────────────────────────────

static void stop_sta(void)
{
    esp_wifi_stop();
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_sta_wifi_handler_instance);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_sta_ip_handler_instance);

    s_connected = false;
    s_is_sta = false;
    return;
}


// Connette alla rete WiFi
static bool start_sta(const char *ssid, const char *password,
                      const char *ip_static)
{

    s_is_sta = true; // Flag per indicare che siamo in modalità STA (anche se non connessi)

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
                                        sta_event_handler, NULL, &s_sta_wifi_handler_instance);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        sta_event_handler, NULL, &s_sta_ip_handler_instance);

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
        s_connected = true;
        ESP_LOGI(TAG, "Connesso a \"%s\"", ssid);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        return true;
    }

    s_is_sta = false; // Flag per indicare che non siamo più in modalità STA
    return false;
}


/*static void check_connection_and_fallback(void* arg)
{
    while (true) {

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_DISCONNECTED_BIT,
                                               pdTRUE, pdFALSE, portMAX_DELAY);

        if (!s_is_ap && (bits & WIFI_DISCONNECTED_BIT) ) {
            ESP_LOGE(TAG, "Connessione STA fallita, fallback a AP");
            stop_sta();
            wifi_start_ap_mode();
            s_is_ap = true;

            break; // Esce da questo task, non serve più

        }

    }

    vTaskDelete(NULL);
}*/

// ─── API pubblica ─────────────────────────────────────────────────────────────

void wifi_init(void)
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

    s_wifi_event_group = xEventGroupCreate();
    s_is_ap = false;
    s_is_sta = false;

    // SSID vuoto → vai direttamente in AP
    if (strlen(ssid.as_str) != 0) {
        // Tenta connessione STA
        ESP_LOGI(TAG, "Connessione a \"%s\"...", ssid.as_str);
        start_sta(ssid.as_str, password.as_str, ip_static.as_str);

        if (!s_connected) {
            ESP_LOGE(TAG, "Connessione STA fallita");
            stop_sta();
        }
        else {
            // Crea task che monitora la connessione STA e fa fallback a AP se cade, non usato ora
            //xTaskCreate(check_connection_and_fallback, "wifi_sta_check_task", 2048, NULL, 5, NULL);
        }
    }
    
    return;
}

bool wifi_is_connected(void)
{
    return s_connected;
}

bool wifi_is_ap(void)
{
    return s_is_ap;
}

bool wifi_is_sta(void)
{
    return s_is_sta;
}

bool wifi_wait_for_connection(void)
{
    bool res = false;

    if (!s_wifi_event_group) {}
    else if (s_connected) res = true;
    else if (s_is_ap ) res = true;
    else {
        xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
        res = s_connected;
    }
    return res;

}
