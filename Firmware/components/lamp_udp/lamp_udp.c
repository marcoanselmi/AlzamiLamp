#include "lamp_udp.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

// ─── Config ───────────────────────────────────────────────────────────────────
#define UDP_RX_BUF_SIZE   256
#define UDP_QUEUE_DEPTH   8
#define UDP_TASK_STACK    4096
#define UDP_TASK_PRIORITY 5

static const char *TAG = "UDP";

extern volatile uint8_t should_exit; // for graceful shutdown

// ─── Module state ─────────────────────────────────────────────────────────────
static QueueHandle_t s_cmd_queue = NULL;

// ─── ACK helpers ─────────────────────────────────────────────────────────────
static void send_ack(int sock, struct sockaddr_in *dest, socklen_t dest_len,
                     bool ok, const char *error)
{
    char buf[96];
    if (ok) {
        snprintf(buf, sizeof(buf), "{\"ok\":true}");
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", error);
    }
    sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)dest, dest_len);
}

// ─── JSON parser ─────────────────────────────────────────────────────────────
static void parse_and_enqueue(const char *data, int len,
                               int sock,
                               struct sockaddr_in *sender,
                               socklen_t sender_len)
{
    char *buf = strndup(data, len);
    if (!buf) {
        ESP_LOGE(TAG, "strndup OOM");
        send_ack(sock, sender, sender_len, false, "out of memory");
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGW(TAG, "Invalid JSON");
        send_ack(sock, sender, sender_len, false, "invalid json");
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd_item)) {
        ESP_LOGW(TAG, "Missing 'cmd' field");
        send_ack(sock, sender, sender_len, false, "missing cmd field");
        cJSON_Delete(root);
        return;
    }

    lamp_cmd_t cmd = {0};
    const char *cs = cmd_item->valuestring;

    if (strcmp(cs, "on") == 0) {
        cmd.type = LAMP_CMD_ON;

    } else if (strcmp(cs, "off") == 0) {
        cmd.type = LAMP_CMD_OFF;

    } else if (strcmp(cs, "set_on_color") == 0) {
        cJSON *r = cJSON_GetObjectItem(root, "r");
        cJSON *g = cJSON_GetObjectItem(root, "g");
        cJSON *b = cJSON_GetObjectItem(root, "b");
        if (!cJSON_IsNumber(r) || !cJSON_IsNumber(g) || !cJSON_IsNumber(b)) {
            send_ack(sock, sender, sender_len, false, "missing r/g/b");
            cJSON_Delete(root);
            return;
        }
        cmd.type = LAMP_CMD_SET_ON_COLOR;
        cmd.color.r = (uint8_t)r->valueint;
        cmd.color.g = (uint8_t)g->valueint;
        cmd.color.b = (uint8_t)b->valueint;

    } else if (strcmp(cs, "set_off_color") == 0) {
        cJSON *r = cJSON_GetObjectItem(root, "r");
        cJSON *g = cJSON_GetObjectItem(root, "g");
        cJSON *b = cJSON_GetObjectItem(root, "b");
        if (!cJSON_IsNumber(r) || !cJSON_IsNumber(g) || !cJSON_IsNumber(b)) {
            send_ack(sock, sender, sender_len, false, "missing r/g/b");
            cJSON_Delete(root);
            return;
        }
        cmd.type = LAMP_CMD_SET_OFF_COLOR;
        cmd.color.r = (uint8_t)r->valueint;
        cmd.color.g = (uint8_t)g->valueint;
        cmd.color.b = (uint8_t)b->valueint;

    } else {
        ESP_LOGW(TAG, "Unknown cmd: %s", cs);
        send_ack(sock, sender, sender_len, false, "unknown cmd");
        cJSON_Delete(root);
        return;
    }

    cJSON_Delete(root);

    send_ack(sock, sender, sender_len, true, NULL);

    if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full — dropping '%s'", cs);
    } else {
        ESP_LOGI(TAG, "Enqueued: %s", cs);
    }
}

// ─── Listener task ────────────────────────────────────────────────────────────
static void udp_listener_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in local = {
        .sin_family      = AF_INET,
        .sin_port        = htons(LAMP_UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Listening on UDP port %d", LAMP_UDP_PORT);

    static char rx_buf[UDP_RX_BUF_SIZE];

    while (!should_exit) {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);

        int n = recvfrom(sock, rx_buf, sizeof(rx_buf) - 1, 0,
                         (struct sockaddr *)&sender, &sender_len);
        if (n < 0) {
            ESP_LOGE(TAG, "recvfrom() failed: errno %d", errno);
            continue;
        }

        rx_buf[n] = '\0';
        ESP_LOGI(TAG, "RX from %s:%d — %s",
                 inet_ntoa(sender.sin_addr),
                 ntohs(sender.sin_port),
                 rx_buf);

        parse_and_enqueue(rx_buf, n, sock, &sender, sender_len);
    }

    close(sock);
    vTaskDelete(NULL);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void udp_start(void)
{
    if (s_cmd_queue) {
        ESP_LOGW(TAG, "udp_start() called more than once — ignoring");
        return;
    }

    s_cmd_queue = xQueueCreate(UDP_QUEUE_DEPTH, sizeof(lamp_cmd_t));
    if (!s_cmd_queue) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return;
    }

    xTaskCreate(udp_listener_task, "udp_listener",
                UDP_TASK_STACK, NULL, UDP_TASK_PRIORITY, NULL);
}

lamp_cmd_t udp_get_command(TickType_t timeout_ms)
{
    lamp_cmd_t cmd = { .type = LAMP_CMD_NONE };
    if (s_cmd_queue) {
        xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(timeout_ms));
    }
    return cmd;
}