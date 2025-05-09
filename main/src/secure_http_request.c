#include "include/secure_http_request.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "rc522_picc.h"

static const char *HTTP_TAG = "http_request";

esp_err_t _http_event_handler(esp_http_client_event_t *event) {
    
    switch(event->event_id) {
        case HTTP_EVENT_ON_DATA:
            if(!esp_http_client_is_chunked_response(event->client)) {
                strncat((char*) event->user_data, (const char *) event->data, event->data_len);
            }
            break;
        default:
            break;
    }

    return ESP_OK;
}

void create_rfid_scan_get_url(const char* uid, char *url) {
    memset(url, 0, sizeof(url));

    char uid_parsed[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];

    uint8_t unparsed_index = 0;
    uint8_t parsed_index = 0;

    while (uid[unparsed_index] != '\0' && parsed_index < sizeof(uid_parsed) - 4) {
        if (uid[unparsed_index] == ' ') {
            uid_parsed[parsed_index++] = '%';
            uid_parsed[parsed_index++] = '2';
            uid_parsed[parsed_index++] = '0';
        } else {
            uid_parsed[parsed_index++] = uid[unparsed_index];
        }
        unparsed_index++;
    }

    // Avsluta strängen
    uid_parsed[parsed_index] = '\0';

    snprintf(url, HTTP_MAX_URL_SIZE, "%s/%s/%s/%s", API_URL, DEVICE_ID, DEVICE_PASSWORD, uid_parsed);

    ESP_LOGI(HTTP_TAG, "Scanned URL has been created: %s", url);
}

/* static void parseJSON(const char* unformatted_string) {
    cJSON *json = cJSON_Parse(unformatted_string);
    
    if(json && cJSON_IsArray(json)) {
        int array_size = cJSON_GetArraySize(json);
        for(int i = 0; i < array_size; i++) {
            cJSON *device = cJSON_GetArrayItem(json, i);
            cJSON *model = cJSON_GetObjectItem(device, "model");
            cJSON *id = cJSON_GetObjectItem(device, "id");
            if(cJSON_IsString(model) && cJSON_IsNumber(id)) {
                ESP_LOGI(HTTP_TAG, "Device ID: %d | Model: %s", id, model);
            }
        }
        cJSON_Delete(json);
    } else {
        ESP_LOGE(HTTP_TAG, "Failed to parse JSON");
    }
} */

void http_get_task(void *pvParameters) {
    http_request_args_t *args = (http_request_args_t*) pvParameters;

    memset(args->response_buffer, 0, sizeof(args->response_buffer));

    ESP_LOGI(HTTP_TAG, "Sending GET request to: %s", args->url);

    esp_http_client_config_t config = {
        .url = args->url,
        .event_handler = _http_event_handler,
        .user_data = args->response_buffer
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t ret = esp_http_client_perform(client);

    if(ret == ESP_OK) {
        ESP_LOGI(HTTP_TAG, "HTTP GET Status = %d", esp_http_client_get_status_code(client));
        ESP_LOGI(HTTP_TAG, "Raw response: %s", args->response_buffer);
        // parseJSON(args->response_buffer);

        if(esp_http_client_get_status_code(client) == 200) {
            args->status = ret;
        }

    } else {
        ESP_LOGE(HTTP_TAG, "HTTP GET request failed: %s", esp_err_to_name(ret));
        args->status = ret;
    }

    esp_http_client_cleanup(client);
    xTaskNotifyGive(args->caller);
    vTaskDelete(NULL);
}

void http_post_task(void *pvParameters) {
    http_request_args_t *args = (http_request_args_t*) pvParameters;
    memset(args->response_buffer, 0, sizeof(args->response_buffer));

    esp_http_client_config_t config = {
        .url = args->url,
        .event_handler = _http_event_handler,
        .user_data = args->response_buffer
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[128] = {};
    snprintf(post_data, sizeof(post_data), "{\"model\":\"%s\"}", DEVICE_ID);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t ret = esp_http_client_perform(client);

    if(ret == ESP_OK) {
        ESP_LOGI(HTTP_TAG, "POST Status: %d", esp_http_client_get_status_code(client));
        ESP_LOGI(HTTP_TAG, "Raw response: %s", args->response_buffer);

        if(esp_http_client_get_status_code(client) == 200) {
            args->status = ret;
        }
    } else {
        ESP_LOGE(HTTP_TAG, "POST request failed: %s", esp_err_to_name(ret));
        args->status = ret;
    }

    esp_http_client_cleanup(client);
    xTaskNotifyGive(args->caller);
    vTaskDelete(NULL);
}