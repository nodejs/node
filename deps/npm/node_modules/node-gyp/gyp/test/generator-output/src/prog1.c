#include <stdio.h>

#include "inc.h"
#include "include1.h"
#include "include2.h"
#include "include3.h"
#include "deeper.h"

int main(int argc, char *argv[])
{
  printf("Hello from prog1.c\n");
  printf("Hello from %s\n", INC_STRING);
  printf("Hello from %s\n", INCLUDE1_STRING);
  printf("Hello from %s\n", INCLUDE2_STRING);
  printf("Hello from %s\n", INCLUDE3_STRING);
  printf("Hello from %s\n", DEEPER_STRING);
  return 0;
}
