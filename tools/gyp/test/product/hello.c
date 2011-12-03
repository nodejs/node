/* Copyright (c) 2009 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

int func1(void) {
  return 42;
}

int main(int argc, char *argv[]) {
  printf("Hello, world!\n");
  printf("%d\n", func1());
  return 0;
}
