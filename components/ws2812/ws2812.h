#ifndef WS2812_H
#define WS2812_H

// Number of LEDs in the chain — change this to match your hardware
#define WS2812_NUM_LEDS   8

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

typedef struct {
    rgb_color_t colors[WS2812_NUM_LEDS];
    uint8_t active[WS2812_NUM_LEDS]; // 1 if LED is active, 0 if off
} ws2812_led_chain_t;

#define WS2812_ALL_OFF ((ws2812_led_chain_t){ .active = {0} })

esp_err_t ws2812_init(void);
esp_err_t ws2812_set_all_color(rgb_color_t color);
esp_err_t ws2812_set_led_chain(ws2812_led_chain_t chain);

#endif // WS2812_H