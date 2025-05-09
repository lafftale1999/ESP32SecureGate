#include "include/rfid_scanner.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_mac.h"
#include <esp_log.h>

static const char *TAG = "rc522-basic-example";

#define RC522_SPI_BUS_GPIO_MISO     GPIO_NUM_19
#define RC522_SPI_BUS_GPIO_MOSI     GPIO_NUM_23
#define RC522_SPI_BUS_GPIO_SCLK     GPIO_NUM_18
#define RC522_SPI_SCANNER_GPIO_SDA  GPIO_NUM_5
#define RC522_SCANNER_GPIO_RST      (-1)

rc522_spi_config_t driver_config = {
    .host_id = SPI2_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
        .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
        .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
    },
    .dev_config = {
        .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
    },
    .rst_io_num = RC522_SCANNER_GPIO_RST,
};

void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    scanned_picc_data_t *scanned_data = (scanned_picc_data_t*)arg;
    if(scanned_data == NULL) {
        ESP_LOGE(TAG, "Unable to parse scanned_data on picc state change");
        return;
    }

    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        rc522_picc_print(picc);
        rc522_picc_uid_to_str(&picc->uid, scanned_data->uid_string, sizeof(scanned_data->uid_string));
        scanned_data->isScanned = true;
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Card has been removed");
    }
}

void rc522_init(rc522_driver_handle_t *driver, rc522_handle_t *scanner, scanned_picc_data_t *scanned_data) {
    
    esp_err_t err = rc522_spi_create(&driver_config, driver);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RC522 SPI driver %s", esp_err_to_name(err));
        return;
    }

    err = rc522_driver_install(*driver);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install RC522 DRIVER: %s", esp_err_to_name(err));
        return;
    }

    rc522_config_t scanner_config = {
        .driver = *driver,
    };

    rc522_create(&scanner_config, scanner);
    rc522_register_events(*scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, scanned_data);
    rc522_start(*scanner);
}