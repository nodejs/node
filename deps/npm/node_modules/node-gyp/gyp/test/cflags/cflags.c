/* Copyright (c) 2010 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

int main(int argc, char *argv[])
{
#ifdef __OPTIMIZE__
  printf("Using an optimization flag\n");
#else
  printf("Using no optimization flag\n");
#endif
  return 0;
}
