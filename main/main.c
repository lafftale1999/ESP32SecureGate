#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/secure_button.h"
#include "include/secure_wifi_driver.h"
#include "include/secure_http_request.h"
#include "secure_https.h"
#include "include/led_response.h"
#include "include/rfid_scanner.h"
#include "nvs_flash.h"

const char *MAIN_TAG = "MAIN";

void app_main(void)
{   
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    static rc522_driver_handle_t driver;
    static rc522_handle_t scanner;
    static scanned_picc_data_t scanned_data;
    scanned_data.isScanned = false;

    rc522_init(&driver, &scanner, &scanned_data);

    wifi_init();
    wait_for_connection();
    led_init();
    
    while(1) {
        if(scanned_data.isScanned) {
            https_request_args_t *https_args = malloc(sizeof(https_request_args_t));
            https_args->caller = xTaskGetCurrentTaskHandle();
            https_args->status = ESP_FAIL;
            https_args->https_request_type = POST;
            https_args->event = TAG_SCANNED;
            
            config_scan_post_request(https_args, &scanned_data);
            xTaskCreate(https_request_task, "http_get_task", 8192, https_args, 5, NULL);

            // Vänta på HTTPS-tasken att bli klar
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            ESP_LOGI(MAIN_TAG, "Current scanned ID: %s", scanned_data.uid_string);
            memset(scanned_data.uid_string, 0, sizeof(scanned_data.uid_string));
            scanned_data.isScanned = false;

            if (https_args->status == ESP_OK) {
                ESP_LOGI(MAIN_TAG, "SUCCESS: %s", https_args->response_buffer);
                blink_green();
            } else {
                ESP_LOGI(MAIN_TAG, "FAILED: %s", https_args->response_buffer);
                blink_red();
            }
            
            free(https_args);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}