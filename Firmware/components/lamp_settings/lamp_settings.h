#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ws2812.h"

/**
 * Struttura che raccoglie tutte le impostazioni persistenti della lampada.
 * Aggiungere campi qui se in futuro servono altre impostazioni.
 */
typedef struct {
    bool    on;
    rgb_color_t on_color;
    rgb_color_t off_color;
} lamp_settings_t;

/**
 * @brief  Inizializza NVS e carica le impostazioni salvate.
 *
 * Deve essere chiamata una volta all'avvio, prima di usare
 * le altre funzioni. Se non ci sono impostazioni salvate
 * applica i valori di default.
 *
 * @param  out  Puntatore alla struttura da riempire con i valori caricati.
 */
void lamp_settings_init(lamp_settings_t *out);

/**
 * @brief  Salva le impostazioni correnti in NVS.
 *
 * Chiamare ogni volta che l'utente cambia colore, luminosità o stato.
 * La scrittura su flash è asincrona internamente ma la funzione
 * aspetta il commit prima di tornare.
 *
 * @param  s  Puntatore alla struttura con i valori da salvare.
 */
void lamp_settings_save(const lamp_settings_t *s);