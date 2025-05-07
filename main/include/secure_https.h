#ifndef


void https_get_request(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST);

void https_get_request_using_cacert_buf(void);

void https_request_task(void *pvparameters);