#include <stdio.h>

extern void common(void);

int main(int argc, char *argv[])
{
  printf("hello from prog1.c\n");
  common();
  return 0;
}
