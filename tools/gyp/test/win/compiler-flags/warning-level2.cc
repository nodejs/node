// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

int f(int x) {
  return 0;
}

int main() {
  double x = 10.1;
  // Cause a level 2 warning (C4243).
  return f(x);
  return 0;
}
