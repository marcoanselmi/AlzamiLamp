#include "lamp_settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "ws2812.h"

static const char *TAG       = "SETTINGS";
static const char *NVS_NS    = "lamp";       // namespace NVS, max 15 caratteri

// Chiavi NVS — max 15 caratteri ciascuna
static const char *KEY_ON    = "on";
static const char *KEY_ON_R  = "on_r";
static const char *KEY_ON_G  = "on_g";
static const char *KEY_ON_B  = "on_b";
static const char *KEY_OFF_R = "off_r";
static const char *KEY_OFF_G = "off_g";
static const char *KEY_OFF_B = "off_b";

// Valori di default applicati se NVS è vuoto
static const lamp_settings_t DEFAULTS = {
    .on         = true,
    .on_color   = {.r = 250, .g = 200, .b = 200},
    .off_color  = {.r = 0,   .g = 0,   .b = 20},
};

// ─── Helpers interni ──────────────────────────────────────────────────────────

static nvs_handle_t open_nvs(nvs_open_mode_t mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NS, mode, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return 0;
    }
    return handle;
}

static uint8_t read_u8(nvs_handle_t h, const char *key, uint8_t fallback)
{
    uint8_t val = fallback;
    esp_err_t err = nvs_get_u8(h, key, &val);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "read '%s' failed: %s", key, esp_err_to_name(err));
    }
    return val;
}

static bool read_bool(nvs_handle_t h, const char *key, bool fallback)
{
    uint8_t val = fallback ? 1 : 0;
    esp_err_t err = nvs_get_u8(h, key, &val);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "read '%s' failed: %s", key, esp_err_to_name(err));
    }
    return val != 0;
}

// ─── API pubblica ─────────────────────────────────────────────────────────────

void lamp_settings_init(lamp_settings_t *out)
{
    if (!out) return;

    // Inizializza NVS — se la partizione è corrotta o aggiornata la cancella
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition issue (%s) — erasing and reinitializing",
                 esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s — using defaults",
                 esp_err_to_name(err));
        *out = DEFAULTS;
        return;
    }

    nvs_handle_t h = open_nvs(NVS_READONLY);
    if (!h) {
        // Namespace non ancora creato — prima volta, usa default
        ESP_LOGI(TAG, "No saved settings found — applying defaults");
        *out = DEFAULTS;
        return;
    }

    out->on         = read_bool(h, KEY_ON,    DEFAULTS.on);
    out->on_color.r = read_u8  (h, KEY_ON_R,  DEFAULTS.on_color.r);
    out->on_color.g = read_u8  (h, KEY_ON_G,  DEFAULTS.on_color.g);
    out->on_color.b = read_u8  (h, KEY_ON_B,  DEFAULTS.on_color.b);
    out->off_color.r = read_u8 (h, KEY_OFF_R, DEFAULTS.off_color.r);
    out->off_color.g = read_u8 (h, KEY_OFF_G, DEFAULTS.off_color.g);
    out->off_color.b = read_u8 (h, KEY_OFF_B, DEFAULTS.off_color.b);

    nvs_close(h);

    ESP_LOGI(TAG, "Loaded: on=%d on_color=(%d,%d,%d) off_color=(%d,%d,%d)",
             out->on, out->on_color.r, out->on_color.g, out->on_color.b,
             out->off_color.r, out->off_color.g, out->off_color.b);
}

void lamp_settings_save(const lamp_settings_t *s)
{
    if (!s) return;

    nvs_handle_t h = open_nvs(NVS_READWRITE);
    if (!h) return;

    esp_err_t err = ESP_OK;

    err |= nvs_set_u8(h, KEY_ON,    s->on ? 1 : 0);
    err |= nvs_set_u8(h, KEY_ON_R,  s->on_color.r);
    err |= nvs_set_u8(h, KEY_ON_G,  s->on_color.g);
    err |= nvs_set_u8(h, KEY_ON_B,  s->on_color.b);
    err |= nvs_set_u8(h, KEY_OFF_R, s->off_color.r);
    err |= nvs_set_u8(h, KEY_OFF_G, s->off_color.g);
    err |= nvs_set_u8(h, KEY_OFF_B, s->off_color.b);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "One or more nvs_set failed");
        nvs_close(h);
        return;
    }

    err = nvs_commit(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Settings saved successfully");
    }

    nvs_close(h);
}