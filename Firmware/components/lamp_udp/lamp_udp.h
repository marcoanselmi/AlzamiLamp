#pragma once
 
/**
 * Avvia il listener UDP.
 * Legge porta e abilitazione da wifi_settings.
 * Se UDP è disabilitato nelle impostazioni non fa nulla.
 * Chiamare DOPO la connessione WiFi.
 */
void udp_start(void);
 