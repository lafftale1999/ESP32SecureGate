#ifndef SECURE_HTTP_REQUEST_H
#define SECURE_HTTP_REQUEST_H

#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

typedef struct {
    const char *url;
    TaskHandle_t caller;
    esp_err_t status;
    char response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1];
} http_request_args_t;

void http_post_task(void *pvParameters);
void http_get_task(void *pvParameters);

#endif