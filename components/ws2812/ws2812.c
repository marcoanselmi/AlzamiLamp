#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_common.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include <math.h>

#include "esp_log.h"
#include "ws2812.h"

extern volatile uint8_t should_exit; // for graceful shutdown

// ---------------------------------------------------------------------------
// WS2812 Hardware Configuration
// ---------------------------------------------------------------------------
#define WS2812_GPIO              GPIO_NUM_1
#define WS2812_RMT_RESOLUTION_HZ 40000000    // 40 MHz => 1 tick = 25 ns

// WS2812 bit timings in nanoseconds
#define WS2812_T0H_NS   350
#define WS2812_T0L_NS   900
#define WS2812_T1H_NS   700
#define WS2812_T1L_NS   600

// WS2812 reset pulse: data line low >50 us latches the colors
#define WS2812_RESET_MS   1     // 1 ms >> 50 us minimum

#define WS2812_BITS_PER_LED  24

// Static GRB buffer for the full LED chain (3 bytes per LED)
static uint8_t s_grb_buf[WS2812_NUM_LEDS * 3];

// ---------------------------------------------------------------------------
// Task / Queue Configuration  (unchanged from original)
// ---------------------------------------------------------------------------
#define WS2812_QUEUE_LEN   1
#define WS2812_DELAY_MS   20    // worker period in ms

// ---------------------------------------------------------------------------
// RMT handles  (module-private)
// ---------------------------------------------------------------------------
static rmt_channel_handle_t s_rmt_channel   = NULL;
static rmt_encoder_handle_t s_rmt_encoder   = NULL;

// ---------------------------------------------------------------------------
// Task / queue handles  (unchanged from original)
// ---------------------------------------------------------------------------
static QueueHandle_t  s_ws_queue       = NULL;
static TaskHandle_t   s_ws_task_handle = NULL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline uint16_t ns_to_ticks(uint32_t ns)
{
    return (uint16_t)((uint64_t)ns * WS2812_RMT_RESOLUTION_HZ / 1000000000UL);
}

// ---------------------------------------------------------------------------
// RMT init  (v5.x API)
// ---------------------------------------------------------------------------
static bool rmt_init(void)
{
    rmt_tx_channel_config_t tx_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .gpio_num          = WS2812_GPIO,
        .mem_block_symbols = 64,          // small; DMA streams the rest
        .resolution_hz     = WS2812_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
        .flags.with_dma    = 1,           // required for single LED and beyond
    };

    if (rmt_new_tx_channel(&tx_cfg, &s_rmt_channel) != ESP_OK) {
        return false;
    }

    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = {
            .level0    = 1,
            .duration0 = ns_to_ticks(WS2812_T0H_NS),
            .level1    = 0,
            .duration1 = ns_to_ticks(WS2812_T0L_NS),
        },
        .bit1 = {
            .level0    = 1,
            .duration0 = ns_to_ticks(WS2812_T1H_NS),
            .level1    = 0,
            .duration1 = ns_to_ticks(WS2812_T1L_NS),
        },
        .flags.msb_first = 1,             // WS2812 expects MSB first
    };

    if (rmt_new_bytes_encoder(&enc_cfg, &s_rmt_encoder) != ESP_OK) {
        rmt_del_channel(s_rmt_channel);
        s_rmt_channel = NULL;
        return false;
    }

    if (rmt_enable(s_rmt_channel) != ESP_OK) {
        rmt_del_encoder(s_rmt_encoder);
        rmt_del_channel(s_rmt_channel);
        s_rmt_channel = NULL;
        s_rmt_encoder = NULL;
        return false;
    }

    return true;
}

static void rmt_cleanup(void)
{
    if (s_rmt_encoder) {
        rmt_del_encoder(s_rmt_encoder);
        s_rmt_encoder = NULL;
    }
    if (s_rmt_channel) {
        rmt_del_channel(s_rmt_channel);
        s_rmt_channel = NULL;
    }
}

// ---------------------------------------------------------------------------
// Gamma correction
// ---------------------------------------------------------------------------

// Gamma correction lookup table for 8-bit input values (0-255)
static uint8_t gamma_table[256];
static void ws2812_init_gamma_table(float gamma)
{
    for (int i = 0; i < 256; i++) {
        float x = (float)i / 255.0f;
        float y = powf(x, gamma);
        int v = (int)(y * 255.0f + 0.5f);

        if (v < 0) v = 0;
        if (v > 255) v = 255;
        gamma_table[i] = (uint8_t)v;
    }
}

static rgb_color_t apply_gamma(rgb_color_t c)
{
    rgb_color_t out = {
        .r = gamma_table[c.r],
        .g = gamma_table[c.g],
        .b = gamma_table[c.b]
    };
    return out;
}

// ---------------------------------------------------------------------------
// Send the same RGB color to all LEDs in the chain.
// Builds a single GRB buffer of WS2812_NUM_LEDS*3 bytes and sends it
// in one RMT transmission — each LED consumes 3 bytes and forwards
// the remainder down the chain automatically.
// ---------------------------------------------------------------------------
static esp_err_t ws2812_transmit_colors(ws2812_led_chain_t chain)
{
    if (s_rmt_channel == NULL || s_rmt_encoder == NULL) {
        return ESP_FAIL;
    }

    // Fill buffer: repeat GRB for every LED in the chain
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
        if (chain.active[i]) {
            rgb_color_t gamma_corrected = apply_gamma(chain.colors[i]);
            s_grb_buf[i * 3 + 0] = gamma_corrected.g;
            s_grb_buf[i * 3 + 1] = gamma_corrected.r;
            s_grb_buf[i * 3 + 2] = gamma_corrected.b;
        } else {
            s_grb_buf[i * 3 + 0] = 0;
            s_grb_buf[i * 3 + 1] = 0;
            s_grb_buf[i * 3 + 2] = 0;
        }
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
    };

    //uint32_t start_time = esp_timer_get_time(); 
    esp_err_t err = rmt_transmit(s_rmt_channel, s_rmt_encoder,
                                  s_grb_buf, sizeof(s_grb_buf), &tx_cfg);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }

    if (rmt_tx_wait_all_done(s_rmt_channel, pdMS_TO_TICKS(200)) != ESP_OK) {
        ESP_LOGW("ws2812", "RMT transmission timeout");
        return ESP_ERR_TIMEOUT;
    }
    //uint32_t duration_ms = ((esp_timer_get_time() )- start_time);
    //ESP_LOGI("ws2812", "RMT transmission completed in %u ms", duration_ms);

    // Reset pulse: hold line low >50 us so all LEDs latch their color
    vTaskDelay(pdMS_TO_TICKS(WS2812_RESET_MS));
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Init and Deinit 
// ---------------------------------------------------------------------------

static esp_err_t ws2812_init(void)
{
    if (!rmt_init()) return ESP_FAIL;

    if (s_ws_queue == NULL) {
        s_ws_queue = xQueueCreate(WS2812_QUEUE_LEN, sizeof(ws2812_led_chain_t));
    }

    if (ws2812_transmit_colors(WS2812_ALL_OFF) != ESP_OK) {  // start with LED off
        rmt_cleanup();
        return ESP_FAIL;
    }

    ws2812_init_gamma_table(2.2f); // typical gamma value for LEDs

    return ESP_OK;
}

static void ws2812_cleanup(void)
{
    if (s_ws_task_handle) {
        vTaskDelete(s_ws_task_handle);
        s_ws_task_handle = NULL;
    }
    if (s_ws_queue) {
        vQueueDelete(s_ws_queue);
        s_ws_queue = NULL;
    }
    rmt_cleanup();
}


// ---------------------------------------------------------------------------
// Worker task: waits for color updates
// ---------------------------------------------------------------------------
void ws2812_task(void *args)
{
    (void)args; // unused
    
    esp_err_t err = ws2812_init();
    if (err != ESP_OK) {
        ESP_LOGE("ws2812", "Initialization failed");
        vTaskDelete(NULL);
    }

    ws2812_led_chain_t incoming_colors;

    while (!should_exit)
    {
        // Drain the queue — higher severity wins
        xQueueReceive(s_ws_queue, &incoming_colors, portMAX_DELAY);
        esp_err_t err = ws2812_transmit_colors(incoming_colors);
        if (err != ESP_OK) {
            ESP_LOGE("ws2812", "Failed to transmit color (%d)", err);
        }
    }

    ws2812_cleanup();
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API 
// ---------------------------------------------------------------------------


esp_err_t ws2812_set_all_color(rgb_color_t color)
{
    ws2812_led_chain_t chain = WS2812_ALL_OFF;
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
        chain.colors[i] = color;
        chain.active[i] = 1;
    }

    if (s_ws_queue == NULL) return ESP_FAIL;
    if (xQueueSend(s_ws_queue, &chain, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ws2812_set_led_chain(ws2812_led_chain_t chain)
{
    if (s_ws_queue == NULL) return ESP_FAIL;
    if (xQueueSend(s_ws_queue, &chain, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    return ESP_OK;
}