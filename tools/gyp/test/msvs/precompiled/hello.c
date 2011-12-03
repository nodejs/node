/* Copyright (c) 2011 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Note the abscence of a stdio.h include.  This will be inserted because of the
// precompiled header.

extern int hello2();

int main(int argc, char *argv[]) {
  printf("Hello, world!\n");
  hello2();
  return 0;
}
