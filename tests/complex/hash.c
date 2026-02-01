#include "hash.h"

static uint32_t fnv1a(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        h ^= (uint8_t)(*s);
        h *= 16777619u;
    }
    return h;
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

void hash_init(Hash* h, Entry* entries, size_t cap) {
    h->entries = entries;
    h->cap = cap;
    for (size_t i = 0; i < cap; i++) {
        h->entries[i].key = (const char*)0;
        h->entries[i].val = 0;
    }
}

int hash_put(Hash* h, const char* key, int32_t v) {
    uint32_t base = fnv1a(key);
    for (size_t i = 0; i < h->cap; i++) {
        size_t idx = (base + (uint32_t)i) % h->cap;
        if (h->entries[idx].key == (const char*)0 || streq(h->entries[idx].key, key)) {
            h->entries[idx].key = key;
            h->entries[idx].val = v;
            return 1;
        }
    }
    return 0;
}

int hash_get(Hash* h, const char* key, int32_t* out) {
    uint32_t base = fnv1a(key);
    for (size_t i = 0; i < h->cap; i++) {
        size_t idx = (base + (uint32_t)i) % h->cap;
        const char* k = h->entries[idx].key;
        if (k == (const char*)0) return 0;
        if (streq(k, key)) { *out = h->entries[idx].val; return 1; }
    }
    return 0;
}
