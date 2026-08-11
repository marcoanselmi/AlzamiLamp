#include "main_logic.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "main_logic.h"
#include "ws2812.h"
#include "wifi.h"
#include "switch.h"
#include "lamp_cmd.h"
#include "lamp_settings.h"

#define LOOP_MS    20
#define FADE_STEP   5
#define WIFI_AP_TIMEOUT_MIN  5 

static inline uint8_t step_toward(uint8_t a, uint8_t d)
{
    if (abs((int)d - (int)a) < FADE_STEP) return d;
    return (uint8_t)(a + FADE_STEP * ((d > a) ? 1 : -1));
}

extern volatile uint8_t should_exit;

static bool _is_on;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static rgb_color_t read_color(const char *key, rgb_color_t fallback)
{
    setting_value_t val;
    if (lamp_settings_get(key, &val)) return val.as_rgb;
    return fallback;
}

static ws2812_led_chain_t make_off_chain(rgb_color_t off_color)
{
    ws2812_led_chain_t chain = WS2812_ALL_OFF;
    chain.colors[0] = off_color;
    chain.active[0] = 1;
    chain.colors[1] = (rgb_color_t){0, 20, 20};
    chain.active[1] = 1;
    return chain;
}

// ─── Task ─────────────────────────────────────────────────────────────────────

void main_logic_task(void *pvParameters)
{
    rgb_color_t on_color  = read_color(SETTING_KEY_ON_COLOR,  (rgb_color_t){250, 200, 200});
    rgb_color_t off_color = read_color(SETTING_KEY_OFF_COLOR, (rgb_color_t){0,   0,   20 });
    rgb_color_t ap_color  = (rgb_color_t){100, 20, 20};
    rgb_color_t sta_color = (rgb_color_t){20, 100, 20};

    ws2812_led_chain_t on_chain  = WS2812_ALL_COLOR(on_color);
    ws2812_led_chain_t off_chain = make_off_chain(off_color);
    ws2812_led_chain_t ap_chain  = WS2812_ALL_COLOR(ap_color);
    ws2812_led_chain_t sta_chain = WS2812_ALL_COLOR(sta_color);

    ws2812_led_chain_t actual_chain  = WS2812_ALL_OFF;
    ws2812_led_chain_t desired_chain = WS2812_ALL_OFF;

    TickType_t last_wake = xTaskGetTickCount();     // Per vTaskDelayUntil, loop periodico
    TickType_t last_switch_on_event = xTaskGetTickCount();  // Per contare gli eventi "on" dello switch fisico
    TickType_t last_ap_command = xTaskGetTickCount();  // Per evitare di riavviare l'AP troppo spesso
    uint8_t switch_on_event_count = 0;      // Contatore di eventi "on" dello switch fisico


    while (!should_exit) {

        // ── Switch fisico ────────────────────────────────────────────────────
        uint8_t event = switch_get_event();
        if (event == 0){    // evento "off" dallo switch fisico
            _is_on = false;
            desired_chain = off_chain;
        }
        else if (event == 1) {  // Evento "on" dallo switch fisico
            _is_on = true;
            desired_chain = on_chain;

            // Controlla se ci sono stati 3 eventi "on" in 3 secondi, accendendo la modalità AP se necessario
            TickType_t now = xTaskGetTickCount();
            if (now - last_switch_on_event < pdMS_TO_TICKS(3000)) {
                switch_on_event_count = (switch_on_event_count + 1) % 250; // evita overflow del contatore
            } else {
                switch_on_event_count = 1; // resetta il contatore se sono passati più di 3 secondi
            }
            last_switch_on_event = now;

            if (switch_on_event_count >= 3 && !wifi_is_ap() && !wifi_is_sta()) {

                ESP_LOGI("MAIN", "Rilevati 3 eventi 'on' in 3 secondi → avvio modalità AP");
                wifi_start_ap_mode();
                last_ap_command = now;
                desired_chain = ap_chain; // accendi la lampada in modalità AP
            }
            else if (switch_on_event_count >= 3 && wifi_is_sta()) {
                desired_chain = sta_chain; // accendi la lampada in modalità STA
            }
        }
        else {
            // Nessun evento, mantieni lo stato attuale
        }

        // ── Comandi dalla coda ────────────────────────────────────────────────
        lamp_cmd_t cmd = lamp_cmd_dequeue(0);

        switch (cmd.type) {

            case LAMP_CMD_NONE:
                break;

            case LAMP_CMD_ON:
                ESP_LOGI("MAIN", "LAMP_CMD_ON");
                _is_on = true;
                desired_chain = on_chain;
                break;

            case LAMP_CMD_OFF:
                ESP_LOGI("MAIN", "LAMP_CMD_OFF");
                _is_on = false;
                desired_chain = off_chain;
                break;

            case LAMP_CMD_SET_ON_COLOR:
                ESP_LOGI("MAIN", "LAMP_CMD_SET_ON_COLOR: r=%d g=%d b=%d", cmd.color.r, cmd.color.g, cmd.color.b);
                on_color = cmd.color;
                on_chain = WS2812_ALL_COLOR(on_color);
                desired_chain = on_chain;
                _is_on = true; // se sto cambiando il colore, assumo che voglia accendere la lampada
                lamp_settings_set(SETTING_KEY_ON_COLOR,
                                  &SETTING_RGB(on_color.r, on_color.g, on_color.b));
                break;

            case LAMP_CMD_SET_OFF_COLOR:
                ESP_LOGI("MAIN", "LAMP_CMD_SET_OFF_COLOR: r=%d g=%d b=%d", cmd.color.r, cmd.color.g, cmd.color.b);
                off_color = cmd.color;
                off_chain = make_off_chain(off_color);
                desired_chain = off_chain;
                _is_on = false; // se sto cambiando il colore, assumo che voglia spegnere la lampada
                lamp_settings_set(SETTING_KEY_OFF_COLOR,
                                  &SETTING_RGB(off_color.r, off_color.g, off_color.b));
                break;

            case LAMP_CMD_SET_SETTING: {
                // Scrivi su NVS nel domain corretto
                bool saved = false;
                if (strcmp(cmd.domain, "lamp") == 0) {
                    saved = lamp_settings_set(cmd.key, &cmd.value);
                } else if (strcmp(cmd.domain, "wifi") == 0) {
                    saved = wifi_settings_set(cmd.key, &cmd.value);
                }

                if (saved) {
                    ESP_LOGI("MAIN", "Setting [%s]/%s salvato", cmd.domain, cmd.key);
                    // Breve delay per dare tempo all'HTTP/UDP di inviare la risposta
                } else {
                    ESP_LOGE("MAIN", "Salvataggio [%s]/%s fallito", cmd.domain, cmd.key);
                }
                break;
            }

            case LAMP_CMD_RESTART:
                ESP_LOGI("MAIN", "LAMP_CMD_RESTART: riavvio in corso...");
                vTaskDelay(pdMS_TO_TICKS(100)); // Attendi 100ms per permettere al trasporto di inviare la risposta ACK
                esp_restart();
                break;

            default:
                break;

        }

        // Aggiorna il timestamp dell'ultimo comando ricevuto in modalità wifi AP
        if (wifi_is_ap() && cmd.type != LAMP_CMD_NONE) {
            last_ap_command = xTaskGetTickCount(); // Aggiorna il timestamp dell'ultimo comando ricevuto in modalità AP
        }
        // Se siamo in modalità AP e sono passati più di 5 minuti dall'ultimo comando ricevuto, spegniamo l'AP
        if (wifi_is_ap() && (xTaskGetTickCount() - last_ap_command > pdMS_TO_TICKS(WIFI_AP_TIMEOUT_MIN * 60 * 1000))) {
            ESP_LOGI("MAIN", "Modalità AP inattiva da %d minuti → spegnimento AP", WIFI_AP_TIMEOUT_MIN);
            wifi_stop_ap_mode();
        }

        // ── Transizione LED ───────────────────────────────────────────────────
        if (memcmp(&actual_chain, &desired_chain, sizeof(ws2812_led_chain_t)) != 0) {

            for (int i = 0; i < WS2812_NUM_LEDS; i++) {

                if (!desired_chain.active[i]) {
                    actual_chain.colors[i].r = (actual_chain.colors[i].r < FADE_STEP) ? 0 : actual_chain.colors[i].r - FADE_STEP;
                    actual_chain.colors[i].g = (actual_chain.colors[i].g < FADE_STEP) ? 0 : actual_chain.colors[i].g - FADE_STEP;
                    actual_chain.colors[i].b = (actual_chain.colors[i].b < FADE_STEP) ? 0 : actual_chain.colors[i].b - FADE_STEP;
                } else {
                    actual_chain.colors[i].r = step_toward(actual_chain.colors[i].r, desired_chain.colors[i].r);
                    actual_chain.colors[i].g = step_toward(actual_chain.colors[i].g, desired_chain.colors[i].g);
                    actual_chain.colors[i].b = step_toward(actual_chain.colors[i].b, desired_chain.colors[i].b);
                }

                actual_chain.active[i] = (actual_chain.colors[i].r >= FADE_STEP ||
                                          actual_chain.colors[i].g >= FADE_STEP ||
                                          actual_chain.colors[i].b >= FADE_STEP) ? 1 : 0;
            }

            ws2812_set_led_chain(actual_chain);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LOOP_MS));
    }

    vTaskDelete(NULL);
}

// ─── API pubblica ─────────────────────────────────────────────────────────────

bool is_lamp_on(void)
{
    return _is_on;
}