#include "lamp_cmd.h"
#include "lamp_settings.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "LAMP_CMD";

// ─── Coda ─────────────────────────────────────────────────────────────────────

static QueueHandle_t s_queue = NULL;

void lamp_cmd_queue_init(void)
{
    if (s_queue) { ESP_LOGW(TAG, "chiamato più volte — ignorato"); return; }
    s_queue = xQueueCreate(LAMP_CMD_QUEUE_DEPTH, sizeof(lamp_cmd_t));
    if (!s_queue) ESP_LOGE(TAG, "Impossibile creare la coda comandi");
}

bool lamp_cmd_enqueue(const lamp_cmd_t *cmd)
{
    if (!s_queue || !cmd) return false;
    if (xQueueSend(s_queue, cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Coda piena — cmd scartato (type=%d)", cmd->type);
        return false;
    }
    return true;
}

lamp_cmd_t lamp_cmd_dequeue(uint32_t timeout_ms)
{
    lamp_cmd_t cmd = {.type = LAMP_CMD_NONE};
    if (s_queue) xQueueReceive(s_queue, &cmd, pdMS_TO_TICKS(timeout_ms));
    return cmd;
}

// ─── Helpers parser ───────────────────────────────────────────────────────────

static void set_err(char *buf, size_t sz, const char *msg)
{
    if (buf && sz > 0) { strncpy(buf, msg, sz - 1); buf[sz - 1] = '\0'; }
}

static bool parse_rgb(cJSON *root, rgb_color_t *out, char *err, size_t err_sz)
{
    cJSON *r = cJSON_GetObjectItem(root, "r");
    cJSON *g = cJSON_GetObjectItem(root, "g");
    cJSON *b = cJSON_GetObjectItem(root, "b");
    if (!cJSON_IsNumber(r) || !cJSON_IsNumber(g) || !cJSON_IsNumber(b)) {
        set_err(err, err_sz, "r/g/b mancanti o non numerici");
        return false;
    }
    out->r = (uint8_t)r->valueint;
    out->g = (uint8_t)g->valueint;
    out->b = (uint8_t)b->valueint;
    return true;
}

// Legge il valore dal JSON in base al tipo atteso dal registro settings
static bool parse_setting_value(cJSON *root, setting_type_t type,
                                setting_value_t *out,
                                char *err, size_t err_sz)
{
    out->type = type;

    switch (type) {
        case SETTING_TYPE_BOOL: {
            cJSON *v = cJSON_GetObjectItem(root, "value");
            if (!cJSON_IsBool(v)) { set_err(err, err_sz, "'value' bool mancante"); return false; }
            out->as_bool = cJSON_IsTrue(v);
            return true;
        }
        case SETTING_TYPE_U8: {
            cJSON *v = cJSON_GetObjectItem(root, "value");
            if (!cJSON_IsNumber(v)) { set_err(err, err_sz, "'value' numerico mancante"); return false; }
            out->as_u8 = (uint8_t)v->valueint;
            return true;
        }
        case SETTING_TYPE_U16: {
            cJSON *v = cJSON_GetObjectItem(root, "value");
            if (!cJSON_IsNumber(v)) { set_err(err, err_sz, "'value' numerico mancante"); return false; }
            out->as_u16 = (uint16_t)v->valueint;
            return true;
        }
        case SETTING_TYPE_RGB: {
            // RGB usa i campi "r","g","b" invece di "value"
            rgb_color_t c;
            if (!parse_rgb(root, &c, err, err_sz)) return false;
            out->as_rgb = c;
            return true;
        }
        case SETTING_TYPE_STRING: {
            cJSON *v = cJSON_GetObjectItem(root, "value");
            if (!cJSON_IsString(v)) { set_err(err, err_sz, "'value' stringa mancante"); return false; }
            strncpy(out->as_str, v->valuestring, SETTING_STR_MAX - 1);
            out->as_str[SETTING_STR_MAX - 1] = '\0';
            return true;
        }
    }

    set_err(err, err_sz, "tipo sconosciuto");
    return false;
}

// ─── Parser JSON ──────────────────────────────────────────────────────────────

bool lamp_cmd_parse_json(const char *json, int len,
                         lamp_cmd_t *out,
                         char *err_out, size_t err_out_size)
{
    if (!json || !out) { set_err(err_out, err_out_size, "parametri null"); return false; }

    char *buf = strndup(json, len > 0 ? (size_t)len : strlen(json));
    if (!buf)  { set_err(err_out, err_out_size, "out of memory"); return false; }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { set_err(err_out, err_out_size, "json non valido"); return false; }

    cJSON *cmd_j = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd_j)) {
        set_err(err_out, err_out_size, "campo 'cmd' mancante");
        cJSON_Delete(root);
        return false;
    }

    const char *cs  = cmd_j->valuestring;
    lamp_cmd_t  cmd = {0};
    bool        ok  = true;

    // ── Comandi real-time LED ────────────────────────────────────────────────

    if (strcmp(cs, "on") == 0) {
        cmd.type = LAMP_CMD_ON;

    } else if (strcmp(cs, "off") == 0) {
        cmd.type = LAMP_CMD_OFF;

    } else if (strcmp(cs, "set_on_color") == 0) {
        cmd.type = LAMP_CMD_SET_ON_COLOR;
        ok = parse_rgb(root, &cmd.color, err_out, err_out_size);

    } else if (strcmp(cs, "set_off_color") == 0) {
        cmd.type = LAMP_CMD_SET_OFF_COLOR;
        ok = parse_rgb(root, &cmd.color, err_out, err_out_size);

    // ── Impostazione generica → NVS + restart ────────────────────────────────

    } else if (strcmp(cs, "set_setting") == 0) {

        cJSON *domain_j = cJSON_GetObjectItem(root, "domain");
        cJSON *key_j    = cJSON_GetObjectItem(root, "key");

        if (!cJSON_IsString(domain_j) || !cJSON_IsString(key_j)) {
            set_err(err_out, err_out_size, "campi 'domain' o 'key' mancanti");
            ok = false;
            goto done;
        }

        const char *domain = domain_j->valuestring;
        const char *key    = key_j->valuestring;

        // Valida il domain
        if (strcmp(domain, "lamp") != 0 && strcmp(domain, "wifi") != 0) {
            set_err(err_out, err_out_size, "domain non valido (usa 'lamp' o 'wifi')");
            ok = false;
            goto done;
        }

        // Recupera il tipo atteso dal registro settings
        // (lamp_settings_get / wifi_settings_get ritornano il tipo corretto)
        setting_value_t current;
        bool found = (strcmp(domain, "lamp") == 0)
                     ? lamp_settings_get(key, &current)
                     : wifi_settings_get(key, &current);

        if (!found) {
            set_err(err_out, err_out_size, "chiave sconosciuta per il domain indicato");
            ok = false;
            goto done;
        }

        // Parsa il valore in base al tipo del registro
        setting_value_t newval;
        ok = parse_setting_value(root, current.type, &newval, err_out, err_out_size);

        if (ok) {
            cmd.type  = LAMP_CMD_SET_SETTING;
            strncpy(cmd.domain, domain, sizeof(cmd.domain) - 1);
            strncpy(cmd.key,    key,    sizeof(cmd.key)    - 1);
            cmd.value = newval;
        }

    } else {
        set_err(err_out, err_out_size, "cmd sconosciuto");
        ok = false;
    }

done:
    cJSON_Delete(root);
    if (ok) {
        *out = cmd;
        ESP_LOGI(TAG, "Parsed ok: cmd=%s", cs);
    }
    return ok;
}