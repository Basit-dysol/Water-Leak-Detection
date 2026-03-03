idf_component_register(SRCS "main.c" "wifi_prov.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_http_server mdns lwip
                       EMBED_TXTFILES "index.html")