#include <stdio.h>

int main(int argc, char *argv[])
{
#if defined(VARIANT1)
  printf("Hello from VARIANT1\n");
#elif  defined(VARIANT2)
  printf("Hello from VARIANT2\n");
#else
  printf("Hello, world!\n");
#endif
  return 0;
}
