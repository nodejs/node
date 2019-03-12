// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should trigger C6001: using uninitialized memory <variable> for |i|.
int f(bool b) {
  int i;
  if (b)
    i = 0;
  return i;
}

int main() {}
