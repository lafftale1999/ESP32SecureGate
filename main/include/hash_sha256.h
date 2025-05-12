#ifndef HASH_SHA256_H
#define HASH_SHA256_H

#include "esp_err.h"

#define SHA256_DIGEST_SIZE 32

esp_err_t hash_sha256(const unsigned char *input_buffer, size_t input_buffer_len, char hex_output_buffer[(SHA256_DIGEST_SIZE * 2) + 1]);

#endif