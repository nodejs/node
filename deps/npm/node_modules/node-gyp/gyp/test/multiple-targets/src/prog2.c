#include <stdio.h>

extern void common(void);

int main(int argc, char *argv[])
{
  printf("hello from prog2.c\n");
  common();
  return 0;
}
