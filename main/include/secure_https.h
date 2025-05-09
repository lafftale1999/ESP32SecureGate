#ifndef SECURE_HTTPS_H
#define SECURE_HTTPS_H

#include "esp_tls.h"
#include "rfid_scanner.h"

#define HTTPS_MAX_URL_SIZE 255
#define MAX_HTTPS_REQUEST_BUFFER 1024
#define MAX_HTTPS_OUTPUT_BUFFER 2048

typedef enum {
    GET,
    POST
} HTTPS_REQUEST_TYPE;


typedef struct {
    char url[HTTPS_MAX_URL_SIZE];
    TaskHandle_t caller;
    esp_err_t status;
    char response_buffer[MAX_HTTPS_OUTPUT_BUFFER + 1];
    char request[MAX_HTTPS_REQUEST_BUFFER + 1];
} https_request_args_t;

void https_get_request(esp_tls_cfg_t cfg, https_request_args_t *args);

void https_get_request_using_cacert_buf(https_request_args_t *args);

void https_request_task(void *pvparameters);

esp_err_t create_check_uid_get_request(https_request_args_t *args, scanned_picc_data_t *data);

#endif