#include <stdio.h>

// Use the assembly function in linux and mac where it is built.
#if PLATFORM_LINUX || PLATFORM_MAC
extern int asm_function(void);
#else
int asm_function() {
  return 41;
}
#endif

int main(void)
{
  fprintf(stdout, "Hello from program.c\n");
  fflush(stdout);
  fprintf(stdout, "Got %d.\n", asm_function());
  fflush(stdout);
  return 0;
}
