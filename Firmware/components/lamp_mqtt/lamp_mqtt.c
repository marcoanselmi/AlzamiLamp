#include "lamp_mqtt.h"
#include "lamp_cmd.h"
#include "lamp_settings.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include <string.h>

#define MQTT_QOS 1

static const char *TAG = "MQTT";

// ─── Stato modulo ─────────────────────────────────────────────────────────────

static esp_mqtt_client_handle_t s_client        = NULL;
static char s_topic_cmd[80]                     = {0};  // "<base>/cmd"
static char s_topic_status[80]                  = {0};  // "<base>/status"
static char s_broker_uri[SETTING_STR_MAX]       = {0};  // copia statica del broker URI

// ─── Event handler ────────────────────────────────────────────────────────────

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connesso al broker");
        esp_mqtt_client_subscribe(s_client, s_topic_cmd, MQTT_QOS);
        // Pubblica online=true appena connesso — annulla l'eventuale LWT precedente
        esp_mqtt_client_publish(s_client, s_topic_status,
                                "{\"online\":true}", 16, MQTT_QOS, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnesso — la libreria riconnette automaticamente");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Sottoscritto a %s", s_topic_cmd);
        break;

    case MQTT_EVENT_DATA: {
        // Ignora messaggi su topic diversi da cmd
        if (strncmp(event->topic, s_topic_cmd, event->topic_len) != 0) break;

        ESP_LOGI(TAG, "RX [%.*s]: %.*s",
                 event->topic_len, event->topic,
                 event->data_len,  event->data);

        // Il payload MQTT non è null-terminated — copiamo in un buffer temporaneo
        // Usiamo un buffer statico — l'event handler è chiamato dal task MQTT,
        // sempre uno alla volta, quindi è thread-safe
        static char rx_buf[256];
        if (event->data_len >= (int)sizeof(rx_buf)) {
            ESP_LOGW(TAG, "Payload troppo lungo (%d byte, max %d) — troncato, comando scartato",
                     event->data_len, (int)sizeof(rx_buf) - 1);
            break;
        }
        int len = event->data_len;
        memcpy(rx_buf, event->data, len);
        rx_buf[len] = '\0';

        // Parsing delegato a lamp_cmd — stesso parser di UDP e HTTP
        lamp_cmd_t cmd;
        static char err[64];
        if (!lamp_cmd_parse_json(rx_buf, &cmd, err, sizeof(err))) {
            ESP_LOGW(TAG, "Parse error: %s", err);
            break;
        }

        // Via MQTT è permesso modificare solo impostazioni del dominio "lamp",
        // non quelle di rete (wifi) — per quelle usare la pagina web
        if (cmd.type == LAMP_CMD_SET_SETTING && strcmp(cmd.domain, "wifi") == 0) {
            ESP_LOGW(TAG, "Modifica impostazioni wifi non permessa via MQTT");
            break;
        }

        lamp_cmd_enqueue(&cmd);
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Errore MQTT — error_type=%d",
                 event->error_handle->error_type);
        break;

    default:
        break;
    }
}

// ─── API pubblica ─────────────────────────────────────────────────────────────

void mqtt_start(void)
{
    if (s_client) {
        ESP_LOGW(TAG, "mqtt_start() chiamato più volte — ignorato");
        return;
    }

    // Controlla se MQTT è abilitato nelle impostazioni
    setting_value_t en;
    if (wifi_settings_get(SETTING_KEY_MQTT_EN, &en) && !en.as_bool) {
        ESP_LOGI(TAG, "MQTT disabilitato nelle impostazioni — skip");
        return;
    }

    // Leggi broker e topic base da wifi_settings
    setting_value_t broker, topic;
    if (!wifi_settings_get(SETTING_KEY_MQTT_BROKER, &broker) ||
        strlen(broker.as_str) == 0) {
        ESP_LOGE(TAG, "Broker URI non configurato — skip");
        return;
    }
    wifi_settings_get(SETTING_KEY_MQTT_TOPIC, &topic);

    // Se topic base è vuoto usa "lamp" come default
    const char *topic_base = (strlen(topic.as_str) > 0) ? topic.as_str : "lamp";

    // Costruisci i topic completi: "<base>/cmd" e "<base>/status"
    snprintf(s_topic_cmd,    sizeof(s_topic_cmd),    "%s/cmd",    topic_base);
    snprintf(s_topic_status, sizeof(s_topic_status), "%s/status", topic_base);

    // Copia broker URI in buffer statico — cfg.broker.address.uri
    // non viene copiato da esp_mqtt_client_init, il puntatore deve restare valido
    strncpy(s_broker_uri, broker.as_str, sizeof(s_broker_uri) - 1);

    // LWT — pubblicato automaticamente dal broker se la lampada si disconnette
    // inaspettatamente (reset, perdita WiFi). Rende il client plug-and-play
    // con Home Assistant e altri sistemi domotici.
    static char lwt_topic[80];
    snprintf(lwt_topic, sizeof(lwt_topic), "%s/status", topic_base);

    ESP_LOGI(TAG, "Connessione a: %s (topic base: %s)", s_broker_uri, topic_base);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri          = s_broker_uri,
        .credentials.client_id       = "AlzamiLamp",
        .session.last_will.topic     = lwt_topic,
        .session.last_will.msg       = "{\"online\":false}",
        .session.last_will.msg_len   = 17,
        .session.last_will.qos       = MQTT_QOS,
        .session.last_will.retain    = 1,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init fallito");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "Client MQTT avviato");
}

void mqtt_publish_status(const char *json_payload)
{
    if (!s_client) {
        ESP_LOGW(TAG, "mqtt_publish_status chiamato prima di mqtt_start");
        return;
    }
    int msg_id = esp_mqtt_client_publish(s_client, s_topic_status,
                                         json_payload, 0, MQTT_QOS, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Publish fallito (non connesso?)");
    } else {
        ESP_LOGD(TAG, "Status pubblicato, msg_id=%d", msg_id);
    }
}