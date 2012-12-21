#include <stdio.h>

int main(int argc, char *argv[])
{
#ifdef BASE
  printf("Base configuration\n");
#endif
#ifdef COMMON
  printf("Common configuration\n");
#endif
#ifdef COMMON2
  printf("Common2 configuration\n");
#endif
#ifdef DEBUG
  printf("Debug configuration\n");
#endif
#ifdef RELEASE
  printf("Release configuration\n");
#endif
  return 0;
}
