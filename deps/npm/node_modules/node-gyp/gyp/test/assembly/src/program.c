#include <stdio.h>

extern int lib1_function(void);

int main(int argc, char *argv[])
{
  fprintf(stdout, "Hello from program.c\n");
  fflush(stdout);
  fprintf(stdout, "Got %d.\n", lib1_function());
  fflush(stdout);
  return 0;
}
