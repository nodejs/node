#include <stdio.h>

extern void function3(void);

int main(void)
{
  printf("Hello from program.c\n");
  function3();
  return 0;
}
