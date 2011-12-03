/* Copyright (c) 2009 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdio.h>

const char *GetToolset();

int main(int argc, char *argv[]) {
  printf("%s\n", GetToolset());
}
