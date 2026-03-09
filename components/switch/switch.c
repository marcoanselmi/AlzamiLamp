#include "switch.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern volatile uint8_t should_exit; // for graceful shutdown

#define SWITCH_GPIO GPIO_NUM_3
#define SWITCH_GPIO_POLL_INTERVAL_MS 20
#define SWITCH_HISTORY_MASK 0b00011111

static const char *TAG = "switch";

static esp_timer_handle_t s_poll_timer = NULL;

static struct{
    uint8_t state;
    uint8_t history; // bitfield: 1 = pressed, 0 = released; LSB is most recent
    int64_t last_change_time;
} s_switch_state = {0, 0, 0};

static void switch_interrupt_handler(void *args)
{
    (void)args; // unused
    s_switch_state.history = ( (s_switch_state.history << 1) | gpio_get_level(SWITCH_GPIO) );
}

static esp_err_t switch_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << SWITCH_GPIO),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    // Set up an interrupt for polling the switch from timer
    // Set timer
    const esp_timer_create_args_t timer_args = {
        .callback = &switch_interrupt_handler,
        .name = "switch_poll_timer"
    };
    err = esp_timer_create(&timer_args, &s_poll_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
        return err;
    }
    // Start periodic timer
    err = esp_timer_start_periodic(s_poll_timer, SWITCH_GPIO_POLL_INTERVAL_MS * 1000); // convert ms to us
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err)); 
        esp_timer_delete(s_poll_timer); // Deinit timer
        s_poll_timer = NULL;
        return err;
    }

    // Initialize switch state history
    s_switch_state.state = gpio_get_level(SWITCH_GPIO);
    s_switch_state.history = (s_switch_state.state) ? 0xFF : 0x00; // Assume stable state at startup
    s_switch_state.last_change_time = esp_timer_get_time();

    return ESP_OK;
}

static void switch_cleanup(void)
{
    // Stop and delete timer
    if (s_poll_timer != NULL) {
        esp_timer_stop(s_poll_timer);
        esp_timer_delete(s_poll_timer);
        s_poll_timer = NULL;
    }
    // Reset GPIO configuration
    gpio_reset_pin(SWITCH_GPIO);
}

void switch_task(void *args)
{
    (void)args; // unused
    esp_err_t err = switch_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Switch initialization failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    while(!should_exit) {
        // Check if switch state has changed (debounced)
        if ( (s_switch_state.history & SWITCH_HISTORY_MASK) == 0x00 ) { // stable pressed
            if (s_switch_state.state == 0) { // was previously released
                s_switch_state.state = 1;
                ESP_LOGI(TAG, "Switch pressed");
            }
        } else if ( (s_switch_state.history & SWITCH_HISTORY_MASK) == SWITCH_HISTORY_MASK ) { // stable released
            if (s_switch_state.state == 1) { // was previously pressed
                s_switch_state.state = 0;
                ESP_LOGI(TAG, "Switch released");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    switch_cleanup(); // Clean up resources
    vTaskDelete(NULL);
}

