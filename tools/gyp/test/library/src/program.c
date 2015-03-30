#include <stdio.h>

extern void lib1_function(void);
extern void lib2_function(void);
extern void moveable_function(void);

int main(void)
{
  fprintf(stdout, "Hello from program.c\n");
  fflush(stdout);
  lib1_function();
  lib2_function();
  moveable_function();
  return 0;
}
