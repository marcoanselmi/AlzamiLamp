#include "lamp_settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SETTINGS";

// ─── Descrittore di una singola impostazione ──────────────────────────────────

typedef struct {
    const char     *key;
    setting_type_t  type;
    setting_value_t def;
} setting_desc_t;

// ─── Registro dominio lampada — namespace "lamp" ──────────────────────────────

static const setting_desc_t LAMP_REGISTRY[] = {
    { SETTING_KEY_ON_COLOR,  SETTING_TYPE_RGB,  SETTING_RGB(250, 200, 200) },
    { SETTING_KEY_OFF_COLOR, SETTING_TYPE_RGB,  SETTING_RGB(0,   0,   20)  },
};

#define LAMP_REGISTRY_SIZE (sizeof(LAMP_REGISTRY) / sizeof(LAMP_REGISTRY[0]))

// ─── Registro dominio wifi/rete — namespace "wifi" ────────────────────────────

static const setting_desc_t WIFI_REGISTRY[] = {
    { SETTING_KEY_SSID,        SETTING_TYPE_STRING, SETTING_STR("")          },
    { SETTING_KEY_PASSWORD,    SETTING_TYPE_STRING, SETTING_STR("")          },
    { SETTING_KEY_IP_STATIC,   SETTING_TYPE_STRING, SETTING_STR("")          },
    { SETTING_KEY_UDP_EN,      SETTING_TYPE_BOOL,   SETTING_BOOL(true)       },
    { SETTING_KEY_UDP_PORT,    SETTING_TYPE_U16,    SETTING_U16(4210)        },
    { SETTING_KEY_MQTT_EN,     SETTING_TYPE_BOOL,   SETTING_BOOL(false)      },
    { SETTING_KEY_MQTT_BROKER, SETTING_TYPE_STRING, SETTING_STR("")          },
    { SETTING_KEY_MQTT_TOPIC,  SETTING_TYPE_STRING, SETTING_STR("lampada")   },
};

#define WIFI_REGISTRY_SIZE (sizeof(WIFI_REGISTRY) / sizeof(WIFI_REGISTRY[0]))

// ─── Stato in RAM ─────────────────────────────────────────────────────────────

static setting_value_t s_lamp_values[LAMP_REGISTRY_SIZE];
static setting_value_t s_wifi_values[WIFI_REGISTRY_SIZE];

static bool s_lamp_initialized = false;
static bool s_wifi_initialized = false;

// ─── Logica interna condivisa ─────────────────────────────────────────────────

static int find_index(const setting_desc_t *registry, int size, const char *key)
{
    for (int i = 0; i < size; i++) {
        if (strcmp(registry[i].key, key) == 0) return i;
    }
    return -1;
}

static nvs_handle_t open_nvs(const char *ns, nvs_open_mode_t mode)
{
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(ns, mode, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open('%s') failed: %s", ns, esp_err_to_name(err));
    }
    return h;
}

static void nvs_read_value(nvs_handle_t h, const setting_desc_t *desc,
                           setting_value_t *out)
{
    *out = desc->def;

    switch (desc->type) {
        case SETTING_TYPE_BOOL:
        case SETTING_TYPE_U8: {
            uint8_t raw;
            if (nvs_get_u8(h, desc->key, &raw) == ESP_OK) {
                out->as_bool = (raw != 0);
                out->as_u8   = raw;
            }
            break;
        }
        case SETTING_TYPE_U16: {
            uint16_t raw;
            if (nvs_get_u16(h, desc->key, &raw) == ESP_OK) {
                out->as_u16 = raw;
            }
            break;
        }
        case SETTING_TYPE_RGB: {
            uint8_t buf[3];
            size_t sz = sizeof(buf);
            if (nvs_get_blob(h, desc->key, buf, &sz) == ESP_OK && sz == 3) {
                out->as_rgb = (rgb_color_t){ buf[0], buf[1], buf[2] };
            }
            break;
        }
        case SETTING_TYPE_STRING: {
            size_t sz = SETTING_STR_MAX;
            nvs_get_str(h, desc->key, out->as_str, &sz);
            break;
        }
    }

    out->type = desc->type;
}

static void nvs_write_value(nvs_handle_t h, const setting_desc_t *desc,
                            const setting_value_t *val)
{
    switch (desc->type) {
        case SETTING_TYPE_BOOL:
            nvs_set_u8(h, desc->key, val->as_bool ? 1 : 0);
            break;
        case SETTING_TYPE_U8:
            nvs_set_u8(h, desc->key, val->as_u8);
            break;
        case SETTING_TYPE_U16:
            nvs_set_u16(h, desc->key, val->as_u16);
            break;
        case SETTING_TYPE_RGB: {
            uint8_t buf[3] = { val->as_rgb.r, val->as_rgb.g, val->as_rgb.b };
            nvs_set_blob(h, desc->key, buf, sizeof(buf));
            break;
        }
        case SETTING_TYPE_STRING:
            nvs_set_str(h, desc->key, val->as_str);
            break;
    }
}

static void _init_domain(const char *ns,
                           const setting_desc_t *registry, int reg_size,
                           setting_value_t *values)
{
    // Use NVS_READWRITE during init to create namespace if it doesn't exist
    nvs_handle_t h = open_nvs(ns, NVS_READWRITE);

    for (int i = 0; i < reg_size; i++) {
        if (h) {
            nvs_read_value(h, &registry[i], &values[i]);
        } else {
            values[i] = registry[i].def;
        }
    }

    if (h) nvs_close(h);
}

static bool settings_get(const setting_desc_t *registry, int reg_size,
                          const setting_value_t *values,
                          const char *ns,
                          const char *key, setting_value_t *out)
{
    int i = find_index(registry, reg_size, key);
    if (i < 0) {
        ESP_LOGW(TAG, "[%s] get: chiave sconosciuta '%s'", ns, key);
        return false;
    }
    *out = values[i];
    return true;
}

static bool settings_set(const char *ns,
                          const setting_desc_t *registry, int reg_size,
                          setting_value_t *values,
                          const char *key, const setting_value_t *value)
{
    int i = find_index(registry, reg_size, key);
    if (i < 0) {
        ESP_LOGW(TAG, "[%s] set: chiave sconosciuta '%s'", ns, key);
        return false;
    }

    if (value->type != registry[i].type) {
        ESP_LOGE(TAG, "[%s] set: tipo errato per '%s' (atteso %d, ricevuto %d)",
                 ns, key, registry[i].type, value->type);
        return false;
    }

    values[i] = *value;

    nvs_handle_t h = open_nvs(ns, NVS_READWRITE);
    if (h) {
        nvs_write_value(h, &registry[i], value);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG, "[%s] set: '%s' salvato", ns, key);
    }

    return true;
}

// ─── API lampada ─────────────────────────────────────────────

bool lamp_settings_get(const char *key, setting_value_t *out)
{
    if (!s_lamp_initialized || !key || !out) return false;
    return settings_get(LAMP_REGISTRY, LAMP_REGISTRY_SIZE, s_lamp_values,
                        "lamp", key, out);
}

bool lamp_settings_set(const char *key, const setting_value_t *value)
{
    if (!s_lamp_initialized || !key || !value) return false;
    return settings_set("lamp", LAMP_REGISTRY, LAMP_REGISTRY_SIZE, s_lamp_values,
                        key, value);
}

// ─── API wifi/network  ──────────────────────────────────────────

bool wifi_settings_get(const char *key, setting_value_t *out)
{
    if (!s_wifi_initialized || !key || !out) return false;
    return settings_get(WIFI_REGISTRY, WIFI_REGISTRY_SIZE, s_wifi_values,
                        "wifi", key, out);
}

bool wifi_settings_set(const char *key, const setting_value_t *value)
{
    if (!s_wifi_initialized || !key || !value) return false;
    return settings_set("wifi", WIFI_REGISTRY, WIFI_REGISTRY_SIZE, s_wifi_values,
                        key, value);
}

// ─── API Generic init -────────────────────────────────────────────────
void settings_init(void)
{
    // Initialize NVS Flash (must be done before opening any NVS namespace)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("MAIN", "Erasing NVS Flash due to initialization error");
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (!s_lamp_initialized) {
        _init_domain("lamp", LAMP_REGISTRY, LAMP_REGISTRY_SIZE, s_lamp_values);
        s_lamp_initialized = true;
        ESP_LOGI(TAG, "Dominio lamp inizializzato (%d impostazioni)", LAMP_REGISTRY_SIZE);
    }

    if (!s_wifi_initialized) {
        _init_domain("wifi", WIFI_REGISTRY, WIFI_REGISTRY_SIZE, s_wifi_values);
        s_wifi_initialized = true;
        ESP_LOGI(TAG, "Dominio wifi inizializzato (%d impostazioni)", WIFI_REGISTRY_SIZE);
    }
}