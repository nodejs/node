#include <stdio.h>

extern void func4(void);

int main(void)
{
  printf("Hello from subdir2/subdir3/subdir4/prog4.c\n");
  func4();
  return 0;
}
