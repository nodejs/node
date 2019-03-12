#include <stdio.h>

void check(int* h, long* k) {
  *h = 1;
  *k = 0;
  printf("%d\n", *h);
}

int main(void) {
  long k;
  check((int*)&k, &k);
  return 0;
}
