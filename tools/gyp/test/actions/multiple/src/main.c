/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

void bar(void);
void car(void);
void dar(void);
void ear(void);

int main() {
  printf("{\n");
  bar();
  car();
  dar();
  ear();
  printf("}\n");
  return 0;
}
