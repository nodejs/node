#include <stdio.h>

#ifdef _WIN32
__declspec(dllexport)
#endif
void module_main(void)
{
  fprintf(stdout, "Hello from lib2.c\n");
  fflush(stdout);
}
