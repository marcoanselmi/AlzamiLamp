#include "lamp_mqtt.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include <string.h>

// ─── Config — move to menuconfig / Kconfig.projbuild in production ────────────
#define MQTT_BROKER_URI    "mqtt://192.168.1.13"
#define MQTT_TOPIC_CMD     "lamp/cmd"
#define MQTT_TOPIC_STATUS  "lamp/status"
#define MQTT_QOS           1
#define MQTT_QUEUE_DEPTH   8

static const char *TAG = "MQTT";

// ─── Module state ─────────────────────────────────────────────────────────────
static esp_mqtt_client_handle_t s_client    = NULL;
static QueueHandle_t            s_cmd_queue = NULL;  // created in mqtt_start()

// ─── Internal helpers ─────────────────────────────────────────────────────────

static void parse_and_enqueue(const char *data, int data_len)
{
    if (!s_cmd_queue) {
        ESP_LOGE(TAG, "Queue not initialized — call mqtt_start() first");
        return;
    }

    char *buf = strndup(data, data_len);
    if (!buf) {
        ESP_LOGE(TAG, "strndup failed — out of memory");
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGW(TAG, "Invalid JSON payload");
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd_item)) {
        ESP_LOGW(TAG, "Missing or invalid 'cmd' field");
        cJSON_Delete(root);
        return;
    }

    lamp_cmd_t cmd = {0};
    const char *cmd_str = cmd_item->valuestring;

    if (strcmp(cmd_str, "on") == 0) {
        cmd.type = LAMP_CMD_ON;

    } else if (strcmp(cmd_str, "off") == 0) {
        cmd.type = LAMP_CMD_OFF;

    } else if (strcmp(cmd_str, "set_on_color") == 0) {
        cJSON *r = cJSON_GetObjectItem(root, "r");
        cJSON *g = cJSON_GetObjectItem(root, "g");
        cJSON *b = cJSON_GetObjectItem(root, "b");
        if (!cJSON_IsNumber(r) || !cJSON_IsNumber(g) || !cJSON_IsNumber(b)) {
            ESP_LOGW(TAG, "Missing r/g/b fields for set_on_color command");
            cJSON_Delete(root);
            return;
        }
        cmd.type = LAMP_CMD_SET_ON_COLOR;
        cmd.color.r = (uint8_t)r->valueint;
        cmd.color.g = (uint8_t)g->valueint;
        cmd.color.b = (uint8_t)b->valueint;

    } else if (strcmp(cmd_str, "set_off_color") == 0) {
        cJSON *r = cJSON_GetObjectItem(root, "r");
        cJSON *g = cJSON_GetObjectItem(root, "g");
        cJSON *b = cJSON_GetObjectItem(root, "b");
        if (!cJSON_IsNumber(r) || !cJSON_IsNumber(g) || !cJSON_IsNumber(b)) {
            ESP_LOGW(TAG, "Missing r/g/b fields for set_off_color command");
            cJSON_Delete(root);
            return;
        }
        cmd.type = LAMP_CMD_SET_OFF_COLOR;
        cmd.color.r = (uint8_t)r->valueint;
        cmd.color.g = (uint8_t)g->valueint;
        cmd.color.b = (uint8_t)b->valueint;

    } else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd_str);
        cJSON_Delete(root);
        return;
    }

    cJSON_Delete(root);

    // Non-blocking send — drop if queue is full so MQTT task never stalls
    if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Command queue full — dropping '%s'", cmd_str);
    }
}

// ─── MQTT event handler ───────────────────────────────────────────────────────

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_CMD, MQTT_QOS);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected — library will reconnect automatically");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed to %s (msg_id=%d)",
                 MQTT_TOPIC_CMD, event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "Unsubscribed (msg_id=%d)", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        if (strncmp(event->topic, MQTT_TOPIC_CMD, event->topic_len) != 0) {
            break;
        }
        ESP_LOGI(TAG, "Received [%.*s]: %.*s",
                 event->topic_len, event->topic,
                 event->data_len,  event->data);
        parse_and_enqueue(event->data, event->data_len);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "Publish confirmed (msg_id=%d)", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error — error_type=%d",
                 event->error_handle->error_type);
        break;

    default:
        break;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void mqtt_start(void)
{
    if (s_client) {
        ESP_LOGW(TAG, "mqtt_start() called more than once — ignoring");
        return;
    }

    // Create the queue internally — caller no longer needs to manage it
    s_cmd_queue = xQueueCreate(MQTT_QUEUE_DEPTH, sizeof(lamp_cmd_t));
    if (!s_cmd_queue) {
        ESP_LOGE(TAG, "Failed to create command queue — out of memory");
        return;
    }

    ESP_LOGI(TAG, "Connecting to: %s", MQTT_BROKER_URI);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        vQueueDelete(s_cmd_queue);   // clean up queue if init fails
        s_cmd_queue = NULL;
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT client started, connecting to %s", MQTT_BROKER_URI);
}

lamp_cmd_t mqtt_get_command(TickType_t timeout_ms)
{
    lamp_cmd_t cmd = { .type = LAMP_CMD_NONE };
    if (s_cmd_queue) {
        xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(timeout_ms));
        // on timeout cmd stays {.type = LAMP_CMD_NONE} — caller checks this
    }
    return cmd;
}

void mqtt_publish_status(const char *json_payload)
{
    if (!s_client) {
        ESP_LOGW(TAG, "mqtt_publish_status called before mqtt_start");
        return;
    }
    int msg_id = esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATUS,
                                         json_payload, 0, MQTT_QOS, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Publish failed (not connected?)");
    } else {
        ESP_LOGD(TAG, "Published status, msg_id=%d", msg_id);
    }
}