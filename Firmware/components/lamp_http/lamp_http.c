#include "lamp_http.h"
#include "lamp_html_page.h"       // stringa HTML embedded
#include "lamp_cmd.h"
#include "lamp_settings.h"
#include "main_logic.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"

#include <string.h>

static const char *TAG = "HTTP";

static bool s_is_ap = false;

// ─── Helper: risposta JSON ────────────────────────────────────────────────────

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t send_ok(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t send_err(httpd_req_t *req, const char *msg)
{
    static char buf[128];
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", msg);
    return send_json(req, buf);
}

// ─── Helper: leggi body POST ──────────────────────────────────────────────────

#define MAX_BODY 512

static int read_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    int total = req->content_len;
    if (total <= 0 || (size_t)total >= buf_size) return -1;

    int received = 0;
    while (received < total) {
        int n = httpd_req_recv(req, buf + received, total - received);
        if (n <= 0) return -1;
        received += n;
    }
    buf[received] = '\0';
    return received;
}

// ─── GET / ────────────────────────────────────────────────────────────────────

static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_sendstr(req, LAMP_PAGE_HTML);
}

// ─── GET /status ──────────────────────────────────────────────────────────────
// Restituisce lo stato completo: mode + impostazioni lampada + impostazioni rete
#define STATUS_BUF_SIZE 640

static esp_err_t handler_status(httpd_req_t *req)
{
    // Leggi tutti i valori prima di costruire il JSON
    static setting_value_t on, on_color, off_color;
    static setting_value_t ssid, ip, udp_en, udp_port, mqtt_en, mqtt_broker, mqtt_topic;
 
    on.type = SETTING_TYPE_BOOL;
    on.as_bool = is_lamp_on();

    on_color = SETTING_RGB(0,0,0);
    off_color = SETTING_RGB(0,0,0);
    lamp_settings_get(SETTING_KEY_ON_COLOR,  &on_color);
    lamp_settings_get(SETTING_KEY_OFF_COLOR, &off_color);

    wifi_settings_get(SETTING_KEY_SSID,        &ssid);
    wifi_settings_get(SETTING_KEY_IP_STATIC,   &ip);
    wifi_settings_get(SETTING_KEY_UDP_EN,      &udp_en);
    wifi_settings_get(SETTING_KEY_UDP_PORT,    &udp_port);
    wifi_settings_get(SETTING_KEY_MQTT_EN,     &mqtt_en);
    wifi_settings_get(SETTING_KEY_MQTT_BROKER, &mqtt_broker);
    wifi_settings_get(SETTING_KEY_MQTT_TOPIC,  &mqtt_topic);
 
    // SSID connesso attualmente (solo in STA, distinto da quello salvato in NVS)
    static char connected_ssid[33] = {0};
    memset(connected_ssid, 0, sizeof(connected_ssid));

    if (!s_is_ap) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            strncpy(connected_ssid, (char *)ap_info.ssid, sizeof(connected_ssid) - 1);
        }
    }
 
    // Costruisci JSON su buffer statico — nessun malloc
    static char buf[STATUS_BUF_SIZE];
    snprintf(buf, sizeof(buf),
        "{" 
        "\"ok\":true,"
        "\"mode\":\"%s\","
        "\"ssid\":\"%s\","
        "\"on\":%s,"
        "\"on_color\":{\"r\":%d,\"g\":%d,\"b\":%d},"
        "\"off_color\":{\"r\":%d,\"g\":%d,\"b\":%d},"
        "\"wifi_ssid\":\"%s\","
        "\"ip_static\":\"%s\","
        "\"udp_en\":%s,"
        "\"udp_port\":%d,"
        "\"mqtt_en\":%s,"
        "\"mqtt_broker\":\"%s\","
        "\"mqtt_topic\":\"%s\""
        "}",
        s_is_ap ? "AP" : "STA",
        connected_ssid,
        on.as_bool       ? "true" : "false",
        on_color.as_rgb.r,  on_color.as_rgb.g,  on_color.as_rgb.b,
        off_color.as_rgb.r, off_color.as_rgb.g, off_color.as_rgb.b,
        ssid.as_str,
        ip.as_str,
        udp_en.as_bool   ? "true" : "false",
        udp_port.as_u16,
        mqtt_en.as_bool  ? "true" : "false",
        mqtt_broker.as_str,
        mqtt_topic.as_str
        // password mai inviata al client per sicurezza
    );
 
    return send_json(req, buf);
}

// ─── POST /cmd ────────────────────────────────────────────────────────────────
// Accetta tutti i comandi: on, off, set_on_color, set_off_color, set_setting
// Via HTTP sono permessi anche i comandi sul dominio "wifi"

static esp_err_t handler_cmd(httpd_req_t *req)
{
    static char body[MAX_BODY];
    memset(body, 0, sizeof(body));

    if (read_body(req, body, sizeof(body)) < 0)
        return send_err(req, "body non valido o troppo lungo");

    ESP_LOGI(TAG, "POST /cmd body: %s", body);

    lamp_cmd_t cmd;
    static char err[64];

    if (!lamp_cmd_parse_json(body, &cmd, err, sizeof(err)))
    {
        ESP_LOGW(TAG, "Parse fallito: %s", err);
        return send_err(req, err);
    }

    ESP_LOGI(TAG, "Cmd type=%d domain=%s key=%s", cmd.type, cmd.domain, cmd.key);

    if (!lamp_cmd_enqueue(&cmd))
        return send_err(req, "coda comandi piena");

    return send_ok(req);
}

// ─── Avvio server ─────────────────────────────────────────────────────────────

void http_server_start(bool is_ap)
{
    s_is_ap = is_ap;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;
    config.stack_size       = 6144;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Impossibile avviare il server HTTP");
        return;
    }

    static const httpd_uri_t routes[] = {
        { .uri = "/",       .method = HTTP_GET,  .handler = handler_root   },
        { .uri = "/status", .method = HTTP_GET,  .handler = handler_status },
        { .uri = "/cmd",    .method = HTTP_POST, .handler = handler_cmd    },
    };

    for (int i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }

    ESP_LOGI(TAG, "Server HTTP avviato (modalita: %s)", is_ap ? "AP" : "STA");
}