idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_eth esp_http_server esp_netif
                    EMBED_FILES index.html) 