idf_component_register(
    SRCS "main.c" "src/secure_button.c" "src/secure_wifi_driver.c" "src/secure_http_request.c" "src/led_response.c" "src/secure_https.c" "src/time_sync.c" "src/rfid_scanner.c" "src/hash_sha256.c"
    INCLUDE_DIRS "." "include"
    EMBED_TXTFILES "certs/secure_gate_server_cert.pem"
)