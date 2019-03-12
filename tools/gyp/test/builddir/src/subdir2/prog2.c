#include <stdio.h>

extern void func2(void);

int main(void)
{
  printf("Hello from subdir2/prog2.c\n");
  func2();
  return 0;
}
