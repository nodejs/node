// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This will cause LNK4254.
#pragma comment(linker, "/merge:.data=.text")

int main() {
  return 0;
}
