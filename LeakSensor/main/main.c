#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

// Configuration
#define WATER_SENSOR_PIN 4
#define DEBOUNCE_DELAY_MS 50

static const char *TAG = "LEAK_DETECTOR";

void app_main(void)
{
    // 1. Configure the GPIO Pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WATER_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,  // Using your external 3.3M resistor
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE      // Polling mode
    };
    gpio_config(&io_conf);

    // 2. State Variables
    int stable_water_state = 1; // HIGH
    int last_raw_state = 1;     // HIGH
    int64_t last_debounce_time = 0;

    ESP_LOGI(TAG, "System Initialized (Filtered Mode)...");

    while (1) {
        // Equivalent to digitalRead()
        int current_raw_reading = gpio_get_level(WATER_SENSOR_PIN);

        // If the pin changed (could be water OR noise)
        if (current_raw_reading != last_raw_state) {
            last_debounce_time = esp_timer_get_time(); // Get time in microseconds
        }

        // If the reading has been stable for longer than our delay (50ms * 1000 = us)
        if ((esp_timer_get_time() - last_debounce_time) > (DEBOUNCE_DELAY_MS * 1000)) {
            
            // If it's different from our last "confirmed" state
            if (current_raw_reading != stable_water_state) {
                stable_water_state = current_raw_reading;

                if (stable_water_state == 0) { // LOW
                    ESP_LOGW(TAG, "--- ALERT: Confirmed Leakage! ---");
                } else {
                    ESP_LOGI(TAG, "Status: Confirmed Dry.");
                }
            }
        }

        last_raw_state = current_raw_reading;

        // Small delay to prevent watchdog issues and save power
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}