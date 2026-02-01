#include <stdint.h>
#include <stddef.h>

typedef struct {
    int32_t x;
    int32_t y;
} Point;

static int32_t dot(const Point* p) {
    return p->x * p->x + p->y * p->y;
}

static void transform(Point* p, int32_t k) {
    p->x = p->x + k;
    p->y = p->y - k;
}

int main(void) {
    Point p = { .x = 3, .y = 4 };

    int32_t acc = 0;
    int32_t arr[5] = {1,2,3,4,5};

    for (int i = 0; i < 5; i++) {
        transform(&p, arr[i]);
        acc += dot(&p);
    }

    int32_t *px = &p.x;
    *px = *px + (acc & 7);

    return (int)(acc + p.x + p.y);
}
