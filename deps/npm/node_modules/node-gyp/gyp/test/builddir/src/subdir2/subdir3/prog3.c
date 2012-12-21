#include <stdio.h>

extern void func3(void);

int main(int argc, char *argv[])
{
  printf("Hello from subdir2/subdir3/prog3.c\n");
  func3();
  return 0;
}
