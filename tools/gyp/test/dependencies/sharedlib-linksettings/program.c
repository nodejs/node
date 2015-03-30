/*
 * Copyright (c) 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

/*
 * This will fail to compile if TEST_DEFINE was propagated from sharedlib to
 * program.
 */
#ifdef TEST_DEFINE
#error TEST_DEFINE is already defined!
#endif

#define TEST_DEFINE 2

extern int staticLibFunc();

int main() {
  printf("%d\n", staticLibFunc());
  printf("%d\n", TEST_DEFINE);
  return 0;
}
