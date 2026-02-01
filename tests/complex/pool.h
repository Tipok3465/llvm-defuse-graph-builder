#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *buf;
  size_t cap;
  size_t off;
} Pool;

void pool_init(Pool *p, uint8_t *backing, size_t cap);
void *pool_alloc(Pool *p, size_t n);
