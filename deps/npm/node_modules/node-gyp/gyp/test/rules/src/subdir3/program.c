#include <stdio.h>

extern void function3(void);

int main(int argc, char *argv[])
{
  printf("Hello from program.c\n");
  function3();
  return 0;
}
