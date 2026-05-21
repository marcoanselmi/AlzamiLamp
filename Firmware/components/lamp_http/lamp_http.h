#pragma once
#include <stdbool.h>

/**
 * Avvia il web server HTTP sulla porta 80.
 *
 *
 * Chiamare DOPO wifi_init().
 * Funziona in entrambe le modalità AP e STA.
 */
void http_server_start();