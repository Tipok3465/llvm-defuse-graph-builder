#include <stdint.h>

static int32_t fib_rec(int32_t n) {
  if (n <= 1) return n;
  return fib_rec(n - 1) + fib_rec(n - 2);
}

static int32_t sum_loop(const int32_t *a, int32_t n) {
  int32_t s = 0;
  for (int32_t i = 0; i < n; ++i) {
    int32_t v = a[i];
    if ((v & 1) == 0) s += v;
    else s -= v;
  }
  return s;
}

int main(void) {
  int32_t k = 7;
  int32_t data[8];
  for (int32_t i = 0; i < 8; ++i) {
    data[i] = i * 3 + 1;
  }
  int32_t s = sum_loop(data, 8);
  int32_t f = fib_rec(k);
  int32_t mixed = s + f;
  int32_t res = mixed & 255;
  return 0;
}
