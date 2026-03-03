#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

// --- Configuration ---
#define I2C_MASTER_SCL_IO           9      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           8      // GPIO number for I2C master data
#define I2C_MASTER_NUM              0      // I2C port number
#define I2C_MASTER_FREQ_HZ          100000 // I2C master clock frequency (100kHz)
#define I2C_MASTER_TX_BUF_DISABLE   0      // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0      // I2C master doesn't need buffer
#define I2C_MASTER_TIMEOUT_MS       1000

#define PCF8574_ADDR                0x20   // Address of the expander

static const char *TAG = "PCF8574";

/**
 * @brief Initialize the ESP32-C3 I2C Master
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Internal pullups (safer if board lacks them)
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        // .clk_flags is optional, 0 is default
    };

    i2c_param_config(i2c_master_port, &conf);
    
    return i2c_driver_install(i2c_master_port, conf.mode, 
                              I2C_MASTER_RX_BUF_DISABLE, 
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

void app_main(void)
{
    // 1. Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully on SDA:%d SCL:%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    // 2. Write 0xFF to PCF8574
    // This sets all pins HIGH (quasi-bidirectional) so buttons can pull them LOW.
    uint8_t config_data = 0xFF;
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, PCF8574_ADDR, 
                                               &config_data, 1, 
                                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Written 0xFF to device. Ready to read P0.");
    } else {
        ESP_LOGE(TAG, "Failed to write to PCF8574. Check wiring/address!");
    }

    // 3. Main Loop
    uint8_t read_data = 0;
    while (1) {
        // Read 1 byte from the device
        ret = i2c_master_read_from_device(I2C_MASTER_NUM, PCF8574_ADDR, 
                                          &read_data, 1, 
                                          I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

        if (ret == ESP_OK) {
            // Check state of Bit 0 (P0)
            // If (data & 1) is 0, the button is connecting P0 to GND.
            if (!(read_data & 0x01)) {
                ESP_LOGI(TAG, "Button on P0 Pressed!");
            }
        } else {
            ESP_LOGE(TAG, "I2C Read Failed");
        }

        // Delay 100ms (equivalent to time.sleep(0.1))
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}