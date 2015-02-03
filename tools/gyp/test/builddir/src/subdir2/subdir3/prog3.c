#include <stdio.h>

extern void func3(void);

int main(void)
{
  printf("Hello from subdir2/subdir3/prog3.c\n");
  func3();
  return 0;
}
