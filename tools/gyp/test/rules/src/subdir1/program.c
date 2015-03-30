#include <stdio.h>

extern void function1(void);
extern void function2(void);

int main(void)
{
  printf("Hello from program.c\n");
  function1();
  function2();
  return 0;
}
