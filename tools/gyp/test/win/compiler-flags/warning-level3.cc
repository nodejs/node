// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cause a level 3 warning (C4359).
struct __declspec(align(8)) C8 { __int64 i; };
struct __declspec(align(4)) C4 { C8 m8; };

int main() {
  return 0;
}
