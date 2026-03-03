#include <stdio.h>
#include <stdint.h>
#include "driver/twai.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TWAI_RX";

#define LED_PIN GPIO_NUM_8

void app_main(void)
{
    // Configure LED pin (GPIO8) as output
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_config);
    gpio_set_level(LED_PIN, 0);  // LED off initially
    ESP_LOGI(TAG, "LED pin configured on GPIO8");

    // TWAI configuration
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_20, GPIO_NUM_21, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        ESP_LOGI(TAG, "TWAI driver installed");
    } else {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
        return;
    }

    // Start driver
    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "TWAI driver started");
    } else {
        ESP_LOGE(TAG, "Failed to start TWAI driver");
        return;
    }

    ESP_LOGI(TAG, "Waiting for CAN messages...");

    twai_message_t message;

    while (1) {
        if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
            ESP_LOGI(TAG, "RX: ID=0x%03lX, DLC=%d, Data: %02X %02X %02X %02X %02X %02X %02X %02X",
                     message.identifier,
                     message.data_length_code,
                     message.data[0], message.data[1], message.data[2], message.data[3],
                     message.data[4], message.data[5], message.data[6], message.data[7]);

            // Check if button press (data[0] == 0x01)
            if (message.data[0] == 0x01) {
                gpio_set_level(LED_PIN, 1);  // Turn ON LED
                ESP_LOGI(TAG, "LED ON - Button pressed on TX side");
            }
            // Check if button release (data[0] == 0x00)
            else if (message.data[0] == 0x00) {
                gpio_set_level(LED_PIN, 0);  // Turn OFF LED
                ESP_LOGI(TAG, "LED OFF - Button released on TX side");
            }
        }
    }
}