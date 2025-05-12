#include "include/secure_https.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"


#include "sdkconfig.h"
#include "include/time_sync.h"
#include "config.h"

static const char *TAG = "HTTPS";

/* Timer interval once every day (24 Hours) */
#define TIME_PERIOD (86400000000ULL)

// Maps out where in the firmware our server certificate exists
extern const uint8_t server_root_cert_pem_start[] asm("_binary_secure_gate_server_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_secure_gate_server_cert_pem_end");

void https_send_request(esp_tls_cfg_t cfg, https_request_args_t *args)
{
    bool got_status_line = false;
    char buf[MAX_HTTPS_OUTPUT_BUFFER + 1];
    uint8_t request_buf_index = 0;
    int ret, len;

    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        return;
    }

    if (esp_tls_conn_http_new_sync(args->url, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        int esp_tls_code = 0, esp_tls_flags = 0;
        esp_tls_error_handle_t tls_e = NULL;
        esp_tls_get_error_handle(tls, &tls_e);
        /* Try to get TLS stack level error and certificate failure flags, if any */
        ret = esp_tls_get_and_clear_last_error(tls_e, &esp_tls_code, &esp_tls_flags);
        if (ret == ESP_OK) {
            ESP_LOGE(TAG, "TLS error = -0x%x, TLS flags = -0x%x", esp_tls_code, esp_tls_flags);
        }
        goto cleanup;
    }

    // Skickar request
    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 args->request + written_bytes,
                                 strlen(args->request) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < strlen(args->request));

    memset(args->response_buffer, 0, MAX_HTTPS_OUTPUT_BUFFER);

    // Läser HTTP svar
    ESP_LOGI(TAG, "Reading HTTP response...");
    do {
        len = sizeof(buf) - 1;
        memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        buf[ret] = '\0'; // nollterminera för strtok

        // 🔍 Läs statusraden från första chunken
        if (!got_status_line) {
            got_status_line = true;
            args->status = ESP_FAIL;

            char *status_line = strtok((char *)buf, "\r\n");
            if (status_line && strncmp(status_line, "HTTP/", 5) == 0) {
                int status_code = atoi(status_line + 9);  // hoppa till status siffran
                ESP_LOGI(TAG, "HTTP status code: %d", status_code);

                args->status = (status_code == 200) ? ESP_OK : ESP_FAIL;
            }
        }

        if(request_buf_index + ret < MAX_HTTPS_OUTPUT_BUFFER - 1){
            for(int i = 0; i < ret; i++) {
                args->response_buffer[request_buf_index++] = buf[i];
            }
    
            args->response_buffer[request_buf_index++] = '\n';
        } else {
            ESP_LOGE(TAG, "Response buffer overflow");
            break;
        }
    } while (1);

    args->response_buffer[request_buf_index++] = '\0';

cleanup:
    esp_tls_conn_destroy(tls);
}

void https_send_with_cert(https_request_args_t *args)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
        .skip_common_name = true
    };
    https_send_request(cfg, args);
}

void https_request_task(void *pvparameters)
{
    https_request_args_t *args = (https_request_args_t*) pvparameters;
    
    https_send_with_cert(args);

    if(args->caller != NULL) {
        xTaskNotifyGive(args->caller);
    }
    vTaskDelete(NULL);
}

void build_request_body(const char** keys, const char **values, size_t amount_of_values, char* request_body) {
    memset(request_body, 0, REQUEST_BODY_SIZE);

    snprintf(request_body, REQUEST_BODY_SIZE, "{");

    for(size_t i = 0; i < amount_of_values; i++) {
        strncat(request_body, "\"", REQUEST_BODY_SIZE - strlen(request_body) - 1);
        strncat(request_body, keys[i], REQUEST_BODY_SIZE - strlen(request_body) - 1);
        strncat(request_body, "\":\"", REQUEST_BODY_SIZE - strlen(request_body) - 1);
        strncat(request_body, values[i], REQUEST_BODY_SIZE - strlen(request_body) - 1);
        strncat(request_body, "\"", REQUEST_BODY_SIZE - strlen(request_body) - 1);
        if(i < amount_of_values - 1) {
            strncat(request_body, ",", REQUEST_BODY_SIZE - strlen(request_body) - 1);
        }
    }

    strncat(request_body, "}", REQUEST_BODY_SIZE - strlen(request_body) - 1);
}

void set_request_path(https_request_args_t *args, const char *path) {
    memset(args->path, 0, MAX_PATH_LENGTH);
    snprintf(args->path, MAX_PATH_LENGTH, "%s", path);
}

void set_request_url(https_request_args_t *args) {
    memset(args->url, 0, HTTPS_MAX_URL_SIZE);
    snprintf(args->url, HTTPS_MAX_URL_SIZE, "https://%s%s", SERVER_HOST, args->path);
}


void log_http_request(const char *request) {
    if (request != NULL) {
        ESP_LOGI(TAG, "Generated HTTP Request:\n%s", request);
    } else {
        ESP_LOGW(TAG, "Request buffer is NULL");
    }
}

esp_err_t build_request(https_request_args_t *args) {
    memset(args->request, 0, MAX_HTTPS_REQUEST_BUFFER +1);
    
    char request_type[10];
    char *content_type = "";
    char content_length[64] = "";

    if(args->https_request_type == POST) {
        snprintf(request_type, sizeof(request_type), "POST");
        content_type = "Content-Type: application/json\r\n";
        snprintf(content_length, sizeof(content_length),
                "Content-Length: %d\r\n", (int)strlen(args->request_body));
    }
    else if(args->https_request_type == GET) {
        snprintf(request_type, sizeof(request_type), "GET");
    }

    int written = snprintf(args->request, MAX_HTTPS_REQUEST_BUFFER + 1,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: esp-idf/1.0 esp32\r\n"
        "Connection: close\r\n"
        "%s"
        "%s"
        "\r\n"
        "%s",
        request_type, args->path, 
        SERVER_HOST,
        content_type,
        content_length,
        (args->https_request_type == POST) ? args->request_body : "");
    
    if (written < 0 || written >= MAX_HTTPS_REQUEST_BUFFER + 1) {
        return ESP_FAIL; // Förfrågan var för lång
    }
    ESP_LOGI(TAG, "%s", args->url);
    log_http_request(args->request);

    return ESP_OK;
}

void config_scan_post_request(https_request_args_t *args, scanned_picc_data_t *data) {
    if (args->https_request_type != POST) {
        ESP_LOGE(TAG, "Called config_scan_post_request() without POST request type set");
        return;
    }

    const char *keys[] = {
        "deviceID",
        "devicePassword",
        "tagID"
    };

    const char *values[] = {
        DEVICE_ID,
        DEVICE_PASSWORD,
        data->sha_256_hex
    };
    build_post_request(args, keys, values, 3);
    
}

void build_post_request(https_request_args_t *args, const char **keys, const char **values, uint8_t list_len) {
    char *event_type = "";

    switch(args->event) {
        case TAG_SCANNED:
            event_type = "/card/scan";
            break;

        default:
            ESP_LOGE(TAG, "Unkown type in build_post_request");
            return;
    }

    build_request_body(keys, values, list_len, args->request_body);
    set_request_path(args, event_type);
    set_request_url(args);
    build_request(args);
}