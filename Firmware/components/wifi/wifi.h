#pragma once
#include <stdbool.h>

/**
 * Inizializza il WiFi leggendo le credenziali da wifi_settings (NVS).
 *
 * Flusso:
 *   SSID vuoto in NVS      → AP mode  → return false
 *   SSID presente          → tenta connessione STA (MAX_RETRY tentativi)
 *     connesso             → return true
 *     fallito              → AP mode  → return false
 *
 * Prerequisiti:
 *   - nvs_flash_init() già chiamato
 *   - wifi_settings_init() già chiamato
 *   - esp_netif_init() e esp_event_loop_create_default() NON ancora chiamati
 */
void wifi_init(void);

/**
 * Ritorna true se la connessione STA è attiva.
 */
bool wifi_is_connected(void);

/**
 * Ritorna true se siamo in modalità AP attiva.
 */
bool wifi_is_ap(void);

/**
 * Ritorna true se siamo in modalità STA connessa.
 */
bool wifi_is_sta(void);

/**
 * Blocca finché la connessione STA non è stabilita o fallita.
 * Ritorna true se connesso, false se fallito.
 * Da chiamare dopo wifi_init() se si vuole aspettare
 * la connessione prima di avviare altri servizi.
 */
bool wifi_wait_for_connection(void);

/**
 * Avvia la modalità AP.
 */
void wifi_start_ap_mode();

/**
 * Ferma la modalità AP.
 */
void wifi_stop_ap_mode();