#include "include/led_response.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define GREEN_LED GPIO_NUM_23
#define RED_LED GPIO_NUM_19

void led_init() {
    gpio_config_t led_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GREEN_LED) | (1ULL << RED_LED)
    };

    gpio_config(&led_config);

    gpio_set_level(GREEN_LED, 0);
    gpio_set_level(RED_LED, 0);
}

void blink_green() {
    gpio_set_level(GREEN_LED, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GREEN_LED, 0);
}

void blink_red() {
    gpio_set_level(RED_LED, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(RED_LED, 0);
}