// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

extern void func(void);
extern void subdir1_func(void);
extern void subdir2_func(void);

int main(void)
{
  printf("Hello from prog3.c\n");
  func();
  subdir1_func();
  subdir2_func();
  return 0;
}
