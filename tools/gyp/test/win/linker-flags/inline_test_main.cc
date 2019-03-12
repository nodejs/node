// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inline_test.h"

#include <intrin.h>
#include <stdio.h>

#pragma intrinsic(_ReturnAddress)

int main() {
  if (IsFunctionInlined(_ReturnAddress()))
    puts("==== inlined ====\n");
}
