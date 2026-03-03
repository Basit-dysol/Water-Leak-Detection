#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "driver/twai.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TWAI_TX";

#define BUTTON_PIN GPIO_NUM_5

void print_twai_status(void)
{
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    
    ESP_LOGI(TAG, "=== TWAI Status ===");
    ESP_LOGI(TAG, "State: %"PRIu32" (0=stopped, 1=running, 2=recovering)", (uint32_t)status_info.state);
    ESP_LOGI(TAG, "TX Errors: %"PRIu32, (uint32_t)status_info.tx_error_counter);
    ESP_LOGI(TAG, "RX Errors: %"PRIu32, (uint32_t)status_info.rx_error_counter);
    ESP_LOGI(TAG, "TX Failed: %"PRIu32, (uint32_t)status_info.tx_failed_count);
    ESP_LOGI(TAG, "RX Missed: %"PRIu32, (uint32_t)status_info.rx_missed_count);
    ESP_LOGI(TAG, "RX Overrun: %"PRIu32, (uint32_t)status_info.rx_overrun_count);
}

void app_main(void)
{
    // Configure button pin (GPIO5) with internal pull-up
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&button_config);
    ESP_LOGI(TAG, "Button pin configured on GPIO5 with pull-up");

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

    vTaskDelay(pdMS_TO_TICKS(1000));
    print_twai_status();

    twai_message_t message = {
        .identifier = 0x123,
        .data_length_code = 8,
        .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    };

    int prev_button_state = 1;

    while (1) {
        int button_state = gpio_get_level(BUTTON_PIN);
        
        // Button pressed (falling edge: 1 -> 0)
        if (prev_button_state == 1 && button_state == 0) {
            ESP_LOGI(TAG, "Button PRESSED!");
            message.data[0] = 0x01;
            
            esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(500));
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "TX: ID=0x%03lX Data: %02X (Button ON)", message.identifier, message.data[0]);
            } else {
                ESP_LOGW(TAG, "Failed to transmit: %s", esp_err_to_name(result));
                print_twai_status();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        // Button released (rising edge: 0 -> 1)
        else if (prev_button_state == 0 && button_state == 1) {
            ESP_LOGI(TAG, "Button RELEASED!");
            message.data[0] = 0x00;
            
            esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(500));
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "TX: ID=0x%03lX Data: %02X (Button OFF)", message.identifier, message.data[0]);
            } else {
                ESP_LOGW(TAG, "Failed to transmit: %s", esp_err_to_name(result));
                print_twai_status();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        prev_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}