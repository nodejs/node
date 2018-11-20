// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-tostring

function testMathToString() {
  assertEquals('[object Math]', "" + Math);
  assertEquals("Math", Math[Symbol.toStringTag]);
  var desc = Object.getOwnPropertyDescriptor(Math, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.writable);
  assertEquals("Math", desc.value);
}
testMathToString();
