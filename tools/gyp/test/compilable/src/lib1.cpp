#include <stdio.h>
#include "lib1.hpp"

void lib1_function(void) {
  fprintf(stdout, "Hello from lib1.c\n");
  fflush(stdout);
}
