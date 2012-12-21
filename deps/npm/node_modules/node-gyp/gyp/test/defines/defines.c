/* Copyright (c) 2011 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

int main(int argc, char *argv[])
{
#ifdef FOO
  printf("FOO is defined\n");
#endif
  printf("VALUE is %d\n", VALUE);

#ifdef PAREN_VALUE
  printf("2*PAREN_VALUE is %d\n", 2*PAREN_VALUE);
#endif

#ifdef HASH_VALUE
  printf("HASH_VALUE is %s\n", HASH_VALUE);
#endif

  return 0;
}
