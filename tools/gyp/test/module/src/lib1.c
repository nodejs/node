#include <stdio.h>

#ifdef _WIN32
__declspec(dllexport)
#endif
void module_main(void)
{
  fprintf(stdout, "Hello from lib1.c\n");
  fflush(stdout);
}
