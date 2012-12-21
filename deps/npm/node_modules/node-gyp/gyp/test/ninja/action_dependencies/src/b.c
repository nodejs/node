/* Copyright (c) 2012 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "b.h"

int main(int argc, char** argv) {
  FILE* f;
  if (argc < 2)
    return 1;
  f = fopen(argv[1], "wt");
  fprintf(f, "#define VALUE %d\n", funcA());
  fclose(f);
  return 0;
}
