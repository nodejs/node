#include <stdio.h>

#ifdef _WIN32
__declspec(dllexport)
#endif
void lib2_function(void)
{
  fprintf(stdout, "Hello from lib2.c\n");
  fflush(stdout);
}
