#ifndef SECURE_HTTPS_H
#define SECURE_HTTPS_H

#include "esp_tls.h"
#include "rfid_scanner.h"

#define HTTPS_MAX_URL_SIZE 255
#define MAX_HTTPS_REQUEST_BUFFER 1024
#define MAX_HTTPS_OUTPUT_BUFFER 2048
#define REQUEST_BODY_SIZE 256
#define MAX_PATH_LENGTH 128

typedef enum {
    GET,
    POST
} HTTPS_REQUEST_TYPE;

typedef enum {
    TAG_SCANNED
} event_fired;


typedef struct {
    char url[HTTPS_MAX_URL_SIZE];
    char path[MAX_PATH_LENGTH];
    TaskHandle_t caller;
    esp_err_t status;
    char response_buffer[MAX_HTTPS_OUTPUT_BUFFER + 1];
    char request[MAX_HTTPS_REQUEST_BUFFER + 1];
    char request_body[REQUEST_BODY_SIZE];
    HTTPS_REQUEST_TYPE https_request_type;
    event_fired event;
} https_request_args_t;

void https_send_request(esp_tls_cfg_t cfg, https_request_args_t *args);

void https_send_with_cert(https_request_args_t *args);

void https_request_task(void *pvparameters);

void build_post_request(https_request_args_t *args, const char **keys, const char **values, uint8_t list_len);
void config_scan_post_request(https_request_args_t *args, scanned_picc_data_t *data);

#endif