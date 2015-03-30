/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

static char data[] = {
#include "cross_program.h"
};

int main(void) {
  fwrite(data, 1, sizeof(data), stdout);
  return 0;
}
