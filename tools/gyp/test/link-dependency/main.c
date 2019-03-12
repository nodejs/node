#include <stdio.h>
#include <stdlib.h>
int main() {
  void *p = malloc(1);
  printf("p: %p\n", p);
  return 0;
}
