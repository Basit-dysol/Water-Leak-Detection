#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "PCA8574_LEAK";

// 1. Define I2C Configuration for ESP32-C3
#define I2C_MASTER_SCL_IO           9      
#define I2C_MASTER_SDA_IO           8      
#define I2C_MASTER_NUM              I2C_NUM_0 
#define I2C_MASTER_FREQ_HZ          100000 
#define PCA8574_ADDR                0x20   

/**
 * @brief Initialize the I2C Master peripheral
 */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void app_main(void) {
    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // 2. Initial setup: Write 0x01 to set P0 as Input
    uint8_t init_data = 0x01;
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, PCA8574_ADDR, &init_data, 1, pdMS_TO_TICKS(100));
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to PCA8574. Check wiring/address.");
    }

    uint8_t read_val;
    while (1) {
        // 3. Read 1 byte from the device
        err = i2c_master_read_from_device(I2C_MASTER_NUM, PCA8574_ADDR, &read_val, 1, pdMS_TO_TICKS(100));

        if (err == ESP_OK) {
            // Check if bit 0 (P0) is 0 (Grounded/Leak)
            if (!(read_val & 0x01)) {
                ESP_LOGW(TAG, "LEAK DETECTED! P0 is LOW (Val: 0x%02X)", read_val);
            } else {
                ESP_LOGI(TAG, "System Dry");
            }
        } else {
            ESP_LOGE(TAG, "I2C Read Error");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
    }
}