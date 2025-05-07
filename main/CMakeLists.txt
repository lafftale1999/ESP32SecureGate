idf_component_register(
    SRCS "main.c" "src/secure_button.c" "src/secure_wifi_driver.c" "src/secure_http_request.c" "src/led_response.c" "src/secure_https.c" "src/time_sync.c"
    INCLUDE_DIRS "." "include"
)