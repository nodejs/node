// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --shared-string-table --harmony-struct

let a = new SharedArray(10);
let desc = Object.getOwnPropertyDescriptor(a, "length");
assertEquals(10, desc.value);
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);

try {
  Object.defineProperty(a, "length", { value: 20 });
  assertUnreachable("Should throw TypeError");
} catch (e) {
  assertTrue(e instanceof TypeError);
}

assertEquals(10, a.length);

// Also try to trigger the C++ assert in debug mode.
let b = new SharedArray(3);
try {
  Object.defineProperty(b, "length", { value: 3 });
} catch (e) {
  // Same value is allowed by Object.defineProperty for non-writable properties!
  // It shouldn't crash.
}
