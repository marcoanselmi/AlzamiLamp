#pragma once
#include <stdbool.h>

/**
 * Avvia il client MQTT.
 * Legge broker URI, topic base e abilitazione da wifi_settings.
 * Se MQTT è disabilitato nelle impostazioni non fa nulla.
 * Chiamare DOPO la connessione WiFi.
 */
void mqtt_start(void);

/**
 * Pubblica lo stato corrente della lampada sul topic <base>/status.
 * Sicuro da chiamare da qualsiasi task.
 */
void mqtt_publish_status(const char *json_payload);