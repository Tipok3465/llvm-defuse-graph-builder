#include "pool.h"

void pool_init(Pool* p, uint8_t* backing, size_t cap) {
    p->buf = backing;
    p->cap = cap;
    p->off = 0;
}

void* pool_alloc(Pool* p, size_t n) {
    if (p->off + n > p->cap) return (void*)0;
    void* r = (void*)(p->buf + p->off);
    p->off += n;
    return r;
}
