#ifndef SECURE_WIFI_DRIVER
#define SECURE_WIFI_DRIVER

#include "config.h"
#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_init();
bool wifi_is_connected();
esp_err_t wait_for_connection(void);

#endif