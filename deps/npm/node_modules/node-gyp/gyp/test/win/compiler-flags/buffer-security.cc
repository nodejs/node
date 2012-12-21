// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <malloc.h>
#include <string.h>

int main() {
  char* stuff = reinterpret_cast<char*>(_alloca(256));
  strcpy(stuff, "blah");
  return 0;
}
