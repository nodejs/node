// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <excpt.h>
#include <stdlib.h>

void fail() {
   try {
      int i = 0, j = 1;
      j /= i;
   } catch(...) {
     exit(1);
   }
}

int main() {
   __try {
      fail();
   } __except(EXCEPTION_EXECUTE_HANDLER) {
     return 2;
   }
   return 3;
}
