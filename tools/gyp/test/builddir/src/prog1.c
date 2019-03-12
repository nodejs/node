#include <stdio.h>

extern void func1(void);

int main(void)
{
  printf("Hello from prog1.c\n");
  func1();
  return 0;
}
