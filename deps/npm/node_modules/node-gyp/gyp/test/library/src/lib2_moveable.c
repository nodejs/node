#include <stdio.h>

#ifdef _WIN32
__declspec(dllexport)
#endif
void moveable_function(void)
{
  fprintf(stdout, "Hello from lib2_moveable.c\n");
  fflush(stdout);
}
