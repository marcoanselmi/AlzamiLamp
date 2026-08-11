#pragma once

#include <stdint.h>

/**
 * Task per la gestione del pulsante.
 * @param args Argomenti per la task.
 */
void switch_task(void *args);

/**
 * Restituisce l'evento del pulsante.
 * @return 0 se il pulsante è rilasciato, 1 se è premuto, 0xFF in caso di mancanza di eventi.
 */
uint8_t switch_get_event(void);