#include <stdio.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_SCL_IO           9      // Your SCL pin
#define I2C_MASTER_SDA_IO           8      // Your SDA pin
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define ADS1115_ADDR                0x48   // ADDR tied to GND

// Register Addresses
#define ADS1115_REG_CONV            0x00
#define ADS1115_REG_CONFIG          0x01

void ads1115_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

float read_ads1115_ain3() {
    /* * CONFIGURATION BREAKDOWN (0x8183):
     * Bit 15: 1     -> Start single-shot conversion
     * Bits 14-12: 111 -> Input is AIN3 (referenced to GND)
     * Bits 11-9: 000  -> PGA Gain set to +/- 6.144V (This is the fix!)
     * Bit 8: 1      -> Single-shot mode
     * Bits 7-5: 100  -> 128 samples per second (default)
     * Bits 4-0: 00011 -> Default comparator settings
     * Resulting Hex: 0xF183 (High: 0xF1, Low: 0x83)
     */
    uint8_t config_data[3] = {ADS1115_REG_CONFIG, 0xF1, 0x83};
    
    // 1. Send configuration to start reading
    i2c_master_write_to_device(I2C_MASTER_NUM, ADS1115_ADDR, config_data, 3, 1000 / portTICK_PERIOD_MS);

    // 2. Wait for conversion (10ms is safe for 128SPS)
    vTaskDelay(pdMS_TO_TICKS(10));

    // 3. Point the internal register pointer to the Conversion Register (0x00)
    uint8_t reg_ptr = ADS1115_REG_CONV;
    i2c_master_write_to_device(I2C_MASTER_NUM, ADS1115_ADDR, &reg_ptr, 1, 1000 / portTICK_PERIOD_MS);

    // 4. Read the 2-byte result
    uint8_t data[2];
    i2c_master_read_from_device(I2C_MASTER_NUM, ADS1115_ADDR, data, 2, 1000 / portTICK_PERIOD_MS);

    // 5. Combine MSB and LSB into a 16-bit signed integer
    int16_t raw_val = (data[0] << 8) | data[1];

    /* * VOLTAGE CALCULATION:
     * Full Scale Range (FSR) = 6.144V
     * 16-bit ADC (signed) = 32768 steps for positive range
     * LSB = 6.144 / 32768 = 0.0001875
     */
    return raw_val * 0.0001875f;
}

void app_main() {
    ads1115_init();
    
    printf("ADS1115 initialized for +/- 6.144V range.\n");

    while (1) {
        float voltage = read_ads1115_ain3();
        printf("AIN3 Voltage: %.4f V\n", voltage);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}