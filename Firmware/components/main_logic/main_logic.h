#pragma once

#include <stdint.h>
#include <stdbool.h>

extern volatile uint8_t should_exit;

void main_logic_task(void *pvParameters);

bool is_lamp_on(void);