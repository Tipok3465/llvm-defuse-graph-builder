#include <stdint.h>
#include <stddef.h>
#include "pool.h"
#include "hash.h"
#include "url.h"

static int32_t checksum32(const uint8_t* p, size_t n) {
    int32_t s = 0;
    for (size_t i = 0; i < n; i++) s = (s * 33) + (int32_t)p[i];
    return s;
}

int main(void) {
    uint8_t backing[2048];
    Pool pool;
    pool_init(&pool, backing, sizeof(backing));

    Entry* entries = (Entry*)pool_alloc(&pool, sizeof(Entry) * 64);
    Hash h;
    hash_init(&h, entries, 64);

    const uint8_t raw[] = { 'a',' ','b','?','x','=','1','&','y','=','2', 0 };
    char* enc = (char*)pool_alloc(&pool, 128);
    size_t enc_n = url_encode(raw, 11, enc, 128);

    int32_t cs = checksum32((const uint8_t*)enc, enc_n);
    hash_put(&h, "encoded_len", (int32_t)enc_n);
    hash_put(&h, "checksum", cs);

    int32_t got = 0;
    int ok = hash_get(&h, "checksum", &got);

    if (!ok) return 111;
    return (int)(got ^ (int32_t)enc_n);
}
