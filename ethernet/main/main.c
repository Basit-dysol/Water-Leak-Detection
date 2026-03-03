#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_http_server.h"

static const char *TAG = "W5500_HTML_FILE";

#define PIN_MISO  5
#define PIN_MOSI  6
#define PIN_SCLK  4
#define PIN_CS    7
#define PIN_INT   3 
#define LED_GPIO  8

static httpd_handle_t server = NULL;

// Access the embedded file pointers
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// Handler for the Root page (index.html)
esp_err_t root_get_handler(httpd_req_t *req) {
    const uint32_t index_html_len = index_html_end - index_html_start;
    
    // Set the content type (so the browser knows it's HTML)
    httpd_resp_set_type(req, "text/html");
    
    // Send the file content
    httpd_resp_send(req, (const char *)index_html_start, index_html_len);
    return ESP_OK;
}

// Handler for the LED Toggle (usually called by a button in your index.html)
esp_err_t toggle_get_handler(httpd_req_t *req) {
    static bool led_state = false;
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state);
    
    ESP_LOGI(TAG, "LED is now %s", led_state ? "ON" : "OFF");

    // After toggling, redirect back to the home page
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void start_webserver() {
    if (server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        // Map "/" to the index.html content
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &root);
        
        // Map "/toggle" for your buttons
        httpd_uri_t toggle = { .uri = "/toggle", .method = HTTP_GET, .handler = toggle_get_handler };
        httpd_register_uri_handler(server, &toggle);
        
        ESP_LOGI(TAG, "Webserver started with index.html");
    }
}

// ... (Rest of your got_ip_event_handler and app_main remain the same as your code) ...

static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Ethernet Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    start_webserver();
}

void app_main(void) {
    gpio_install_isr_service(0);
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO, .mosi_io_num = PIN_MOSI, .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    vTaskDelay(pdMS_TO_TICKS(100));

    spi_device_interface_config_t devcfg = {
        .command_bits = 16, .address_bits = 8, .mode = 0,
        .clock_speed_hz = 20 * 1000 * 1000, .spics_io_num = PIN_CS, .queue_size = 20
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = PIN_INT;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1; 
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    uint8_t base_mac_addr[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
    mac->set_addr(mac, base_mac_addr);

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    esp_netif_attach(eth_netif, glue);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}