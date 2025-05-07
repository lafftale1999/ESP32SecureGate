#ifndef SECURE_BUTTON_H
#define SECURE_BUTTON_H

#include "config.h"
#include "esp_err.h"

extern volatile int is_clicked;

esp_err_t button_init();
void IRAM_ATTR button_isr_handler(void* arg);
void debounce_timer_callback(TimerHandle_t xTimer);

#endif