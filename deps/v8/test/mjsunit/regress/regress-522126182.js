// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestLargeArrayJoinWithSeparator() {
  let len = 0xfffffffc;
  let a = new Array(len);
  a[0] = "A".repeat(0x100000);  // Force slow path loop (dictionary elements)
  assertThrows(() => a.join(","), RangeError);
})();

(function TestLargeArrayJoinEmptySeparator() {
  let len = 0xfffffffc;
  let a = new Array(len);
  // Should not throw because separator is empty, and array has no elements.
  assertEquals("", a.join(""));
})();
