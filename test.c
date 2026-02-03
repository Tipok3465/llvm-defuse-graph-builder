#include <stdint.h>

// TODO[DKay]: You skip some intresting test-cases. Try up some loops and
// recursive functions.

// TODO[flops]: It's better to locate it in tests/ (end to end) or use it as
// example [flops]: UPD it's just a copy of tests/simple/simple.c so it's better
// to remove it completely

static int compute(int a, int b) {
  int sum = a + b;
  int sum2 = sum + 2;
  int prod = a * b;

  if (sum > prod)
    return sum;
  return prod;
}

int main(void) {
  int r = compute(2, 5);
  return r - 10;
}
