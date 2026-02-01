#include "url.h"

static int is_unreserved(uint8_t c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == '-' || c == '_' || c == '.' || c == '~') return 1;
    return 0;
}

static char hex(int v) {
    return (char)(v < 10 ? ('0' + v) : ('A' + (v - 10)));
}

size_t url_encode(const uint8_t* in, size_t n, char* out, size_t out_cap) {
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = in[i];
        if (is_unreserved(c)) {
            if (w + 1 >= out_cap) break;
            out[w++] = (char)c;
        } else {
            if (w + 3 >= out_cap) break;
            out[w++] = '%';
            out[w++] = hex((c >> 4) & 0xF);
            out[w++] = hex(c & 0xF);
        }
    }
    if (w < out_cap) out[w] = 0;
    return w;
}
