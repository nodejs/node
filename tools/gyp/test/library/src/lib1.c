#include <stdio.h>

#ifdef _WIN32
__declspec(dllexport)
#endif
void lib1_function(void)
{
  fprintf(stdout, "Hello from lib1.c\n");
  fflush(stdout);
}
