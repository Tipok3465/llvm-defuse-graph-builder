#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char *key;
  int32_t val;
} Entry;

typedef struct {
  Entry *entries;
  size_t cap;
} Hash;

void hash_init(Hash *h, Entry *entries, size_t cap);
int hash_put(Hash *h, const char *key, int32_t v);
int hash_get(Hash *h, const char *key, int32_t *out);
