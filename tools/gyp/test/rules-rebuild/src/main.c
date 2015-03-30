#include <stdio.h>

extern void prog1(void);
extern void prog2(void);

int main(void)
{
  printf("Hello from main.c\n");
  prog1();
  prog2();
  return 0;
}
