#include <stdio.h>

extern void common(void);

int main(void)
{
  printf("hello from prog2.c\n");
  common();
  return 0;
}
