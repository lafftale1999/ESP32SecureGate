#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "include/secure_button.h"

#define BUTTON_GPIO GPIO_NUM_26
#define DEBOUNCE_TIME_MS 50

TimerHandle_t debounce_timer;
static const char *BUTTON_TAG = "Button";
static bool isr_service_installed = false;
volatile int is_clicked = 0;

esp_err_t button_init() {
    
    gpio_config_t button_config = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    
    esp_err_t ret = gpio_config(&button_config);

    debounce_timer = xTimerCreate(
        "debounce_timer",
        pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
        pdFALSE, NULL, debounce_timer_callback
    );

    if(debounce_timer == NULL) {
        ret = ESP_FAIL;
        ESP_LOGE(BUTTON_TAG, "Error initiating debounce_timer");
    }

    if(!isr_service_installed) {
        ret = gpio_install_isr_service(0);
        isr_service_installed = true;
    }

    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

    ESP_LOGI(BUTTON_TAG, "Button succesfully initiated");
    return ret;
}

void IRAM_ATTR button_isr_handler(void* arg) {
    if(!xTimerIsTimerActive(debounce_timer)) {
        xTimerStartFromISR(debounce_timer, NULL);
    }
}

void debounce_timer_callback(TimerHandle_t xTimer) {
    if(gpio_get_level(BUTTON_GPIO) == 0) {
        ESP_LOGI(BUTTON_TAG, "Button clicked!");
        is_clicked = 1;
    }
}