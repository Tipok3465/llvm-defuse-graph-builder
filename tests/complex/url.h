#pragma once
#include <stddef.h>
#include <stdint.h>

size_t url_encode(const uint8_t *in, size_t n, char *out, size_t out_cap);
