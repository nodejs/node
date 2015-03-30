#include <stdio.h>
#include "define3.h"
#include "define4.h"

extern void function1(void);
extern void function2(void);
extern void function3(void);
extern void function4(void);

int main(void)
{
  printf("Hello from program.c\n");
  function1();
  function2();
  printf("%s", STRING3);
  printf("%s", STRING4);
  return 0;
}
