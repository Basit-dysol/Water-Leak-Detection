#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "mdns.h"
#include "lwip/inet.h"
#include "esp_http_server.h"

static const char *TAG = "wifi_prov";

// These variables point to your embedded index.html file in memory
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/* ======================================================================
   NVS MEMORY HELPER FUNCTIONS
   ====================================================================== */
void save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t my_handle;
    ESP_LOGI(TAG, "Saving credentials to NVS...");
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
        nvs_set_str(my_handle, "ssid", ssid);
        nvs_set_str(my_handle, "password", password);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Credentials saved successfully!");
    }
}

bool load_wifi_credentials(char *ssid, char *password, size_t max_len) {
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READONLY, &my_handle) != ESP_OK) {
        return false;
    }

    size_t ssid_len = max_len;
    size_t pass_len = max_len;
    
    esp_err_t err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(my_handle);
        return false;
    }
    
    nvs_get_str(my_handle, "password", password, &pass_len);
    nvs_close(my_handle);
    return true;
}

void clear_wifi_credentials(void) {
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
        nvs_erase_all(my_handle);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/* ======================================================================
   WIFI EVENT HANDLER (For Station Mode)
   ====================================================================== */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Failed to connect to router! Erasing credentials and reverting to AP mode...");
        clear_wifi_credentials();
        esp_restart();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "===============================================");
        ESP_LOGI(TAG, "SUCCESS! Connected to router.");
        ESP_LOGI(TAG, "ESP32 IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "===============================================");
    }
}

/* ======================================================================
   LEAK DETECTION APP WEBSERVER (Connected Mode)
   ====================================================================== */
static esp_err_t app_root_get_handler(httpd_req_t *req) {
    // Calculate the size of the embedded HTML file
    const size_t html_size = (index_html_end - index_html_start);
    
    httpd_resp_set_type(req, "text/html");
    // Send the embedded file directly to the browser
    httpd_resp_send(req, (const char *)index_html_start, html_size);
    return ESP_OK;
}

static void start_app_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t app_root = { 
            .uri = "/", 
            .method = HTTP_GET, 
            .handler = app_root_get_handler, 
            .user_ctx = NULL 
        };
        httpd_register_uri_handler(server, &app_root);
        ESP_LOGI(TAG, "Leak Detection App Webserver Started!");
    }
}

/* ======================================================================
   CAPTIVE PORTAL WEBSERVER (Setup Mode)
   ====================================================================== */
static esp_err_t chat_post_handler(httpd_req_t *req) {
    char buf[200];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[64] = {0};
    char password[64] = {0};

    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "&password=");

    if (ssid_start && pass_start) {
        ssid_start += 5;
        int ssid_len = pass_start - ssid_start;
        strncpy(ssid, ssid_start, ssid_len);

        pass_start += 10;
        strcpy(password, pass_start);

        ESP_LOGI(TAG, "Received SSID: '%s'", ssid);
        ESP_LOGI(TAG, "Received PASS: '%s'", password);

        // Save to NVS before rebooting!
        save_wifi_credentials(ssid, password);

        const char *resp = "Credentials Received! Rebooting to connect...";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    return ESP_OK;
}

static esp_err_t setup_root_get_handler(httpd_req_t *req) {
    const char* html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                       "<style>body{font-family:Arial,sans-serif; text-align:center; margin-top:50px; background-color:#f4f4f4;}"
                       "input{margin:10px; padding:10px; width:80%; max-width:300px; border-radius:5px; border:1px solid #ccc;}"
                       "button{padding:10px 20px; background-color:#28a745; color:white; border:none; border-radius:5px; cursor:pointer;}</style></head>"
                       "<body><h2>Device Setup</h2><form action=\"/chat\" method=\"POST\">"
                       "<input type=\"text\" name=\"ssid\" placeholder=\"Wi-Fi Name\" required><br>"
                       "<input type=\"password\" name=\"password\" placeholder=\"Password\" required><br>"
                       "<button type=\"submit\">Connect</button></form></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_setup_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/*", .method = HTTP_GET, .handler = setup_root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t chat_uri = { .uri = "/chat", .method = HTTP_POST, .handler = chat_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &chat_uri);
    }
}

/* ======================================================================
   MAIN ENTRY POINT
   ====================================================================== */
void app_wifi_init(void) {
    // 1. Initialize NVS (Memory)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};

    // 2. Branching Logic: Check Memory
    if (load_wifi_credentials(saved_ssid, saved_pass, sizeof(saved_ssid))) {
        // PATH A: Credentials found. Connect to Router!
        ESP_LOGI(TAG, "Found saved credentials! Connecting to %s...", saved_ssid);

        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.sta.ssid, saved_ssid);
        strcpy((char *)wifi_config.sta.password, saved_pass);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());

        // ---> START LEAK DETECTION DASHBOARD <---
        start_app_webserver();

    } else {
        // PATH B: Memory empty. Start Captive Portal!
        ESP_LOGI(TAG, "No credentials found. Starting setup Hotspot...");

        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = "MyDevice_Setup",
                .ssid_len = strlen("MyDevice_Setup"),
                .channel = 1,
                .password = "",
                .max_connection = 4,
                .authmode = WIFI_AUTH_OPEN
            },
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        mdns_init();
        mdns_hostname_set("setup");
        
        // ---> START SETUP CAPTIVE PORTAL <---
        start_setup_webserver();
        ESP_LOGI(TAG, "Wi-Fi AP started. Connect to SSID: MyDevice_Setup");
    }
}