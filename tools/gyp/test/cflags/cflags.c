/* Copyright (c) 2010 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

int main(void)
{
#ifdef FOO
  printf("FOO defined\n");
#else
  printf("FOO not defined\n");
#endif
  return 0;
}
