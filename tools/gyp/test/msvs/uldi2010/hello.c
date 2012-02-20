/* Copyright (c) 2012 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

extern int hello2();

int main(int argc, char *argv[]) {
  printf("Hello, world!\n");
  hello2();
  return 0;
}
