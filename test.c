#include <stdint.h>

static int compute(int a, int b) {
    int sum = a + b;
    int sum2 = sum + 2;
    int prod = a * b;

    if (sum > prod) return sum;
    return prod;
}

int main(void) {
    int r = compute(2, 5);
    return r - 10;
}
