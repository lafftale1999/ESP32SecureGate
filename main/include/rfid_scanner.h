#ifndef RFID_SCANNER_H
#define RFID_SCANNER_H

#include "include/rfid_scanner.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_mac.h"
#include <esp_log.h>
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"

void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data);

void rc522_init(rc522_driver_handle_t *driver, rc522_handle_t *scanner);

#endif