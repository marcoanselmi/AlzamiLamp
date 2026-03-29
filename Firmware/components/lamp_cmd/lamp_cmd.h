#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ws2812.h"
#include "lamp_settings.h"

// ─── Tipi comando ─────────────────────────────────────────────────────────────

typedef enum {
    LAMP_CMD_NONE = 0,
    LAMP_CMD_ON,
    LAMP_CMD_OFF,
    LAMP_CMD_SET_ON_COLOR,     // payload: color
    LAMP_CMD_SET_OFF_COLOR,    // payload: color
    LAMP_CMD_SET_SETTING,      // payload: domain + key + value → NVS 
    LAMP_CMD_RESTART,          // restart after setting 
} lamp_cmd_type_t;

typedef struct {
    lamp_cmd_type_t type;

    // LAMP_CMD_SET_ON_COLOR / LAMP_CMD_SET_OFF_COLOR
    rgb_color_t color;

    // LAMP_CMD_SET_SETTING
    char            domain[8];   // "lamp" oppure "wifi"
    char            key[16];     // chiave NVS, es. "ssid", "udp_port"
    setting_value_t value;       // valore generico con union taggata
} lamp_cmd_t;

// ─── Coda condivisa ───────────────────────────────────────────────────────────

#define LAMP_CMD_QUEUE_DEPTH 8

/**
 * Crea la coda. Chiamare UNA SOLA VOLTA in app_main prima di avviare
 * qualsiasi trasporto o il task main_logic.
 */
void lamp_cmd_queue_init(void);

/**
 * Invia un comando sulla coda (non bloccante).
 * Restituisce true se accodato, false se piena.
 */
bool lamp_cmd_enqueue(const lamp_cmd_t *cmd);

/**
 * Riceve il prossimo comando dalla coda.
 * timeout_ms=0 → non bloccante.
 * Restituisce LAMP_CMD_NONE se la coda è vuota.
 */
lamp_cmd_t lamp_cmd_dequeue(uint32_t timeout_ms);

// ─── Parser JSON condiviso ────────────────────────────────────────────────────
//
// Comandi real-time:
//   {"cmd":"on"}
//   {"cmd":"off"}
//   {"cmd":"set_on_color",  "r":250, "g":200, "b":200}
//   {"cmd":"set_off_color", "r":0,   "g":0,   "b":20}
//
// Impostazioni generiche (→ NVS + restart gestito da main_logic):
//   {"cmd":"set_setting", "domain":"wifi", "key":"ssid",        "value":"MiaRete"}
//   {"cmd":"set_setting", "domain":"wifi", "key":"udp_port",    "value":4210}
//   {"cmd":"set_setting", "domain":"wifi", "key":"mqtt_en",     "value":true}
//   {"cmd":"set_setting", "domain":"lamp", "key":"on_color",    "r":250, "g":200, "b":200}
//
// Restituisce true se il parsing ha successo.
// In caso di errore scrive la descrizione in err_out (può essere NULL).

bool lamp_cmd_parse_json(const char *json, lamp_cmd_t *out,
                         char *err_out, size_t err_out_size);