// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

int main() {
  const int i = -1;
  // Cause a level 4 warning (C4245).
  unsigned int j = i;
  return 0;
}
