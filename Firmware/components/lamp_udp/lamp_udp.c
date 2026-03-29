#include "lamp_udp.h"
#include "lamp_cmd.h"
#include "lamp_settings.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define UDP_RX_BUF_SIZE   256
#define UDP_TASK_STACK    4096
#define UDP_TASK_PRIORITY 5

static const char *TAG = "UDP";

extern volatile uint8_t should_exit;

// ─── ACK ──────────────────────────────────────────────────────────────────────

static void send_ack(int sock, struct sockaddr_in *dest, socklen_t dest_len,
                     bool ok, const char *error)
{
    char buf[96];
    if (ok) {
        snprintf(buf, sizeof(buf), "{\"ok\":true}");
    } else {
        snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", error ? error : "");
    }
    sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)dest, dest_len);
}

// ─── Listener task ────────────────────────────────────────────────────────────

static void udp_listener_task(void *arg)
{
    uint16_t port = (uint16_t)(uintptr_t)arg;

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
        .sin_port        = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "In ascolto su porta UDP %d", port);

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

        ESP_LOGI(TAG, "RX da %s:%d — %s",
                 inet_ntoa(sender.sin_addr), ntohs(sender.sin_port), rx_buf);

        // Parsing delegato a lamp_cmd
        lamp_cmd_t cmd;
        char err[64];
        if (!lamp_cmd_parse_json(rx_buf, &cmd, err, sizeof(err))) {
            ESP_LOGW(TAG, "Parse error: %s", err);
            send_ack(sock, &sender, sender_len, false, err);
            continue;
        }

        // Via UDP è permesso modificare solo impostazioni del dominio "lamp",
        // non quelle di rete (wifi) — per quelle usare la pagina web
        if (cmd.type == LAMP_CMD_SET_SETTING && strcmp(cmd.domain, "wifi") == 0) {
            ESP_LOGW(TAG, "Modifica impostazioni wifi non permessa via UDP");
            send_ack(sock, &sender, sender_len, false, "wifi settings non modificabili via UDP");
            continue;
        }

        send_ack(sock, &sender, sender_len, true, NULL);
        lamp_cmd_enqueue(&cmd);
    }

    close(sock);
    vTaskDelete(NULL);
}

// ─── API pubblica ─────────────────────────────────────────────────────────────

void udp_start(void)
{
    
    
    // Controlla se UDP è abilitato nelle impostazioni
    setting_value_t en;
    if (!wifi_settings_get(SETTING_KEY_UDP_EN, &en)) {
        ESP_LOGW(TAG, "Impossibile leggere impostazione UDP_EN — assuming disabled");
        return;
    }
    ESP_LOGI(TAG, "udp_start() called");
    
    if (!en.as_bool) {
        ESP_LOGI(TAG, "UDP disabilitato nelle impostazioni — skip");
        return;
    }

    // Leggi la porta dalle impostazioni
    setting_value_t port_val;
    uint16_t port = 4210; // default di fallback
    if (!wifi_settings_get(SETTING_KEY_UDP_PORT, &port_val)) {
        ESP_LOGW(TAG, "Impossibile leggere UDP_PORT — using default %d", port);
    } else {
        port = port_val.as_u16;
    }

    // Passa la porta al task come argomento (cast a puntatore, tecnica standard FreeRTOS)
    BaseType_t result = xTaskCreate(udp_listener_task, "udp_listener",
                UDP_TASK_STACK, (void *)(uintptr_t)port, UDP_TASK_PRIORITY, NULL);
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Errore creazione task UDP listener");
        return;
    }
    
    ESP_LOGI(TAG, "UDP listener task creato — porta: %d", port);
}