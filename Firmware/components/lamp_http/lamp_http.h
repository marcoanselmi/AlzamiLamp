#pragma once
#include <stdbool.h>

/**
 * Avvia il web server HTTP sulla porta 80.
 *
 * @param is_ap  true  → modalità Access Point (sezioni lampada disabilitate/grigie)
 *               false → modalità Station (tutto abilitato)
 *
 * Chiamare DOPO wifi_init().
 * Funziona in entrambe le modalità AP e STA.
 */
void http_server_start(bool is_ap);