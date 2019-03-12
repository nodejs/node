/* Copyright (c) 2009 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

const char *GetToolset();
const char *GetToolsetShared();

int main(void) {
  printf("%s\n", GetToolset());
  printf("Shared: %s\n", GetToolsetShared());
}
