#pragma once
#include "esp_err.h"

// Define our operating modes
typedef enum {
    PROV_MODE_AP,
    PROV_MODE_STA
} prov_mode_t;

void app_wifi_init(void);