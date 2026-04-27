// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var p = "A";
for (var i = 0; i < 15000; i++) p = "(" + p + ")";

try {
  // Should not crash with a stack overflow.
  new RegExp(p).test("A");
} catch (e) {
  // Depending on the C++ compile options the stack frames
  // may be so big that we get this error.
  assertTrue(String(e).indexOf("Regular expression too large") > 0);
}
