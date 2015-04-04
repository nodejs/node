#include <stdio.h>

extern void func(void);

int main(void)
{
  printf("Hello from prog2.c\n");
  func();
  /*
   * Uncomment to test same-named files in different directories,
   * which Visual Studio doesn't support.
  subdir1_func();
  subdir2_func();
   */
  return 0;
}
