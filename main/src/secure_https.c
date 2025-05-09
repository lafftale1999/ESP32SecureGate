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

void https_get_request(esp_tls_cfg_t cfg, https_request_args_t *args)
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

void https_get_request_using_cacert_buf(https_request_args_t *args)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
        .skip_common_name = true
    };
    https_get_request(cfg, args);
}

void https_request_task(void *pvparameters)
{
    https_request_args_t *args = (https_request_args_t*) pvparameters;

    ESP_LOGI(TAG, "************ START HTTP REQUEST ************");

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
    https_get_request_using_cacert_buf(args);
    ESP_LOGI(TAG, "************ END HTTP REQUEST ************");

    if(args->caller != NULL) {
        xTaskNotifyGive(args->caller);
    }
    vTaskDelete(NULL);
}

void parseUID(const char* uid, char uid_to_parse[RC522_PICC_UID_STR_BUFFER_SIZE_MAX]) {
    uint8_t unparsed_index = 0;
    uint8_t parsed_index = 0;

    while (uid[unparsed_index] != '\0' && parsed_index < RC522_PICC_UID_STR_BUFFER_SIZE_MAX - 4) {
        if (uid[unparsed_index] == ' ') {
            uid_to_parse[parsed_index++] = '%';
            uid_to_parse[parsed_index++] = '2';
            uid_to_parse[parsed_index++] = '0';
        } else {
            uid_to_parse[parsed_index++] = uid[unparsed_index];
        }
        unparsed_index++;
    }

    // Avsluta strängen
    uid_to_parse[parsed_index] = '\0';
}

esp_err_t create_check_uid_get_request(https_request_args_t *args, scanned_picc_data_t *data) {
    // Format UID for url
    char url_parsed_uid[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];
    parseUID(data->uid_string, url_parsed_uid);

    // Create PATH for request
    char request_path[256];
    int path_length = snprintf(request_path, sizeof(request_path), 
        "/%s/%s/%s", DEVICE_ID, DEVICE_PASSWORD, url_parsed_uid);

    if(path_length <= 0 || path_length >= sizeof(request_path)) {
        return ESP_FAIL; // path too long or not initiated
    }

    // Create URL for endpoint
    memset(args->url, 0, HTTPS_MAX_URL_SIZE);
    int url_len = snprintf(args->url, HTTPS_MAX_URL_SIZE, "%s/%s/%s/%s", SERVER_ROOT, DEVICE_ID, DEVICE_PASSWORD, url_parsed_uid);
    if (url_len < 0 || url_len >= HTTPS_MAX_URL_SIZE) {
        return ESP_FAIL;
    }

    // FORMAT REQUEST
    memset(args->request, 0, MAX_HTTPS_REQUEST_BUFFER +1);
    int written = snprintf(args->request, MAX_HTTPS_REQUEST_BUFFER + 1,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: esp-idf/1.0 esp32\r\n"
        "Connection: close\r\n"
        "\r\n", request_path ,SERVER_HOST);
    
    if (written < 0 || written >= MAX_HTTPS_REQUEST_BUFFER + 1) {
        return ESP_FAIL; // Förfrågan var för lång
    }

    return ESP_OK;
}