#include <stdio.h>

#include "inc.h"
#include "include1.h"
#include "include2.h"
#include "shadow.h"

int main(void)
{
  printf("Hello from includes.c\n");
  printf("Hello from %s\n", INC_STRING);
  printf("Hello from %s\n", INCLUDE1_STRING);
  printf("Hello from %s\n", INCLUDE2_STRING);
  /* Test that include_dirs happen first: The gyp file has a -Ishadow1
     cflag and an include_dir of shadow2.  Including shadow.h should get
     the shadow.h from the include_dir. */
  printf("Hello from %s\n", SHADOW_STRING);
  return 0;
}
