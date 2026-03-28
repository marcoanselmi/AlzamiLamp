#include "switch.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"

extern volatile uint8_t should_exit; // for graceful shutdown

#define SWITCH_GPIO GPIO_NUM_1
#define SWITCH_GPIO_DEBOUNCE_MS 200

static const char *TAG = "switch";

static esp_timer_handle_t s_poll_timer = NULL;
static QueueHandle_t s_switch_event_queue = NULL;

static struct{
    uint8_t state;
    int64_t last_change_time;
} s_switch_state = {0, 0};

static void IRAM_ATTR switch_interrupt_handler(void *args)
{
    if ( s_switch_state.state != gpio_get_level(SWITCH_GPIO) )
    {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR( (TaskHandle_t)args, &woken ); // Increment notification count for the task
        portYIELD_FROM_ISR(woken);
    }
}

static esp_err_t switch_init(void)
{
    gpio_config_t io_conf = {
        .mode           = GPIO_MODE_INPUT,
        .pin_bit_mask   = (1ULL << SWITCH_GPIO),
        .pull_down_en   = GPIO_PULLDOWN_ENABLE,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .intr_type      = GPIO_INTR_ANYEDGE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    s_switch_event_queue = xQueueCreate(1, sizeof(uint8_t));
    if (s_switch_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_FAIL;
    }

    // Set up an interrupt for changes in switch state
    err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        return err;
    }
    err = gpio_isr_handler_add(SWITCH_GPIO, switch_interrupt_handler, (void *)xTaskGetCurrentTaskHandle());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(err));
        return err;
    }

    // Initialize switch state history
    s_switch_state.state = gpio_get_level(SWITCH_GPIO);
    s_switch_state.last_change_time = esp_timer_get_time();

    // Enable wakeup from light sleep on switch GPIO
    gpio_wakeup_enable( (1ULL << SWITCH_GPIO), s_switch_state.state ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);

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

    xQueueSend(s_switch_event_queue, &s_switch_state.state, 0); // Send initial state

    while(!should_exit) {
        // Wait for notifications from ISR (block indefinitely, for sleep mode)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(portMAX_DELAY));

        ESP_LOGI(TAG, "Switch interrupt received");
            
        // wait for a short debounce period
        vTaskDelay(pdMS_TO_TICKS(SWITCH_GPIO_DEBOUNCE_MS)); // Polling interval

        // Check if the switch state has changed since the last check
        if (s_switch_state.state != gpio_get_level(SWITCH_GPIO)) {
            s_switch_state.state = gpio_get_level(SWITCH_GPIO);
            s_switch_state.last_change_time = esp_timer_get_time();

            xQueueSend(s_switch_event_queue, &s_switch_state.state, 0);

            ESP_LOGI(TAG, "Switch state changed: %s", s_switch_state.state ? "upright" : "tilted");
            gpio_wakeup_enable( (1ULL << SWITCH_GPIO), s_switch_state.state ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
        }

        ulTaskNotifyTake(pdTRUE, 0); // Clear any additional notifications that may have come in during debounce
    }

    switch_cleanup(); // Clean up resources
    vTaskDelete(NULL);
}

uint8_t switch_get_event(void)
{
    uint8_t event;
    if (xQueueReceive(s_switch_event_queue, &event, 0) == pdTRUE) {
        return event; // 1 for tilted, 0 for upright
    }
    return 0xFF; // No event
}