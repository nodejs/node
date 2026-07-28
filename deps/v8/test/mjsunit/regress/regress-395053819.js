// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let o = {}
for (var i = 0; i < 10000; i++) {
  o[8589933568 + i] = i;
}
for (var i = 0; i < 10000; i++) {
  assertEquals(o[8589933568 + i], i);
}
