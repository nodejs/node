/* Copyright (c) 2014 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

extern const char* getString(void);

int main(int argc, char* argv[])
{
  if (argc < 2) return 2;
  FILE* f = fopen(argv[1], "w");
  if (f == NULL) return 1;
  fprintf(f, "%s", getString());
  fclose(f);
  return 0;
}
