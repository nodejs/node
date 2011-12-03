#include <stdio.h>

extern void func1(void);

int main(int argc, char *argv[])
{
  printf("Hello from prog1.c\n");
  func1();
  return 0;
}
