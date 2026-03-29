#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ws2812.h"

// ─── Tipi di valore supportati ────────────────────────────────────────────────

#define SETTING_STR_MAX 64

typedef enum {
    SETTING_TYPE_BOOL,
    SETTING_TYPE_U8,
    SETTING_TYPE_U16,
    SETTING_TYPE_RGB,
    SETTING_TYPE_STRING,
} setting_type_t;

typedef struct {
    setting_type_t type;
    union {
        bool        as_bool;
        uint8_t     as_u8;
        uint16_t    as_u16;
        rgb_color_t as_rgb;
        char        as_str[SETTING_STR_MAX];
    };
} setting_value_t;

// ─── Nomi ddomini nvs ────────────────────────────────────────────────────────────────
#define LAMP_NVS_NAMESPACE "lamp_settings"
#define WIFI_NVS_NAMESPACE "wifi_settings"

// ─── Chiavi dominio lampada ───────────────────────────────────────────────────
#define SETTING_KEY_ON_COLOR    "on_color"
#define SETTING_KEY_OFF_COLOR_ENABLED "off_color_enabled"
#define SETTING_KEY_OFF_COLOR   "off_color"

// ─── Chiavi dominio wifi/rete ─────────────────────────────────────────────────

#define SETTING_KEY_SSID        "ssid"
#define SETTING_KEY_PASSWORD    "password"
#define SETTING_KEY_IP_STATIC   "ip_static"    // stringa vuota = DHCP
#define SETTING_KEY_UDP_EN      "udp_en"
#define SETTING_KEY_UDP_PORT    "udp_port"
#define SETTING_KEY_MQTT_EN     "mqtt_en"
#define SETTING_KEY_MQTT_BROKER "mqtt_broker"  // es. "mqtt://192.168.1.10"
#define SETTING_KEY_MQTT_TOPIC  "mqtt_topic"   // es. "lampada"

// ─── Helpers per costruire un setting_value_t inline ─────────────────────────

#define SETTING_BOOL(v)    ((setting_value_t){ .type = SETTING_TYPE_BOOL,   .as_bool = (v)           })
#define SETTING_U8(v)      ((setting_value_t){ .type = SETTING_TYPE_U8,     .as_u8   = (v)           })
#define SETTING_U16(v)     ((setting_value_t){ .type = SETTING_TYPE_U16,    .as_u16  = (v)           })
#define SETTING_RGB(r,g,b) ((setting_value_t){ .type = SETTING_TYPE_RGB,    .as_rgb  = {(r),(g),(b)} })
#define SETTING_STR(s)     ((setting_value_t){ .type = SETTING_TYPE_STRING, .as_str  = (s)           })

// ─── API dominio lampada ──────────────────────────────────────────────────────

/**
 * Carica le impostazioni lampada da NVS (namespace "lamp").
 * Da chiamare UNA SOLA VOLTA in app_main dopo nvs_flash_init().
 */
bool lamp_settings_get(const char *key, setting_value_t *out);
bool lamp_settings_set(const char *key, const setting_value_t *value);

// ─── API dominio wifi/rete ────────────────────────────────────────────────────

/**
 * Carica le impostazioni di rete da NVS (namespace "wifi").
 * Da chiamare UNA SOLA VOLTA in app_main dopo nvs_flash_init().
 */
bool wifi_settings_get(const char *key, setting_value_t *out);
bool wifi_settings_set(const char *key, const setting_value_t *value);

// ─── API Generic init -────────────────────────────────────────────────
/**
 * Inizializza entrambi i domini (lamp e wifi) caricando i valori da NVS.
 * Da chiamare UNA SOLA VOLTA in app_main dopo nvs_flash_init().
 */
void settings_init(void);