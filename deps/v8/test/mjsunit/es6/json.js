// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-tostring

function testJSONToString() {
  assertEquals('[object JSON]', "" + JSON);
  assertEquals("JSON", JSON[Symbol.toStringTag]);
  var desc = Object.getOwnPropertyDescriptor(JSON, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.writable);
  assertEquals("JSON", desc.value);
}
testJSONToString();
