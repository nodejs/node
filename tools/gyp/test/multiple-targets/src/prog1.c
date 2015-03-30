#include <stdio.h>

extern void common(void);

int main(void)
{
  printf("hello from prog1.c\n");
  common();
  return 0;
}
