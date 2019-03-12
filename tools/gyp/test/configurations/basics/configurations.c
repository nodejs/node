#include <stdio.h>

int main(void)
{
#ifdef FOO
  printf("Foo configuration\n");
#endif
#ifdef DEBUG
  printf("Debug configuration\n");
#endif
#ifdef RELEASE
  printf("Release configuration\n");
#endif
  return 0;
}
