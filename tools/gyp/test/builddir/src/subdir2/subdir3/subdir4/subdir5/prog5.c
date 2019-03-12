#include <stdio.h>

extern void func5(void);

int main(void)
{
  printf("Hello from subdir2/subdir3/subdir4/subdir5/prog5.c\n");
  func5();
  return 0;
}
