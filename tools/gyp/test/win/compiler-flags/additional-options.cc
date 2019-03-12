// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

int main() {
  // Generate a warning that will appear at level 4, but not level 1
  // (truncation and unused local).
  char c = 123456;
  return 0;
}
