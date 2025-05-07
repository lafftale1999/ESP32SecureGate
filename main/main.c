#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/secure_button.h"
#include "include/secure_wifi_driver.h"
#include "include/secure_http_request.h"
#include "include/led_response.h"
#include "nvs_flash.h"

void app_main(void)
{   
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    wait_for_connection();
    button_init();
    led_init();
    
    while(1) {
        if(is_clicked) {
            http_request_args_t *http_args = malloc(sizeof(http_request_args_t));
            http_args->url = API_EP_ADD;
            http_args->caller = xTaskGetCurrentTaskHandle();
            http_args->status = ESP_FAIL;

            xTaskCreate(http_post_task, "http_post_task", 4096, http_args, 5, NULL);

            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            is_clicked = 0;

            if(http_args->status == ESP_OK) {
                blink_green();
            } else {
                blink_red();
            }

            free(http_args);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}