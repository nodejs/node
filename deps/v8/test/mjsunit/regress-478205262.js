// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var re_src = "(?=^)";
for (var i = 0; i < 1000; i++) {
  re_src = "(?=(?:" + re_src + "|foo))";
}
try {
  // Should not crash with a stack overflow.
  var re = new RegExp(re_src);
  re.test("sdfdsfsdfdfs");
} catch (e) {
  // Depending on the C++ compile options the stack frames
  // may be so big that we get this error.
  assertTrue(String(e).indexOf("Regular expression too large") > 0);
}
