// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Object literal with large-object elements.
let indices = [];
const max = 0x1ef3e + 100;
for (let i = 0; i < max; i++) {
  indices.push(""+i);
}
let source = "return {" + indices.join(":0,") + ":0};"
let largeElementsLiteral = new Function(source);

function TestLargeObjectElements() {
  // Corresponds to FixedArray::kMaxRegularLength.
  let object = largeElementsLiteral();
  %HeapObjectVerify(object);
  for (let i = 0; i < max; i++) {
    assertEquals(0, object[i]);
  }
  object[0] = 0xFF;
  assertEquals(0xFF, object[0]);
  object[1] = 1.23;
  assertEquals(1.23, object[1]);
  %HeapObjectVerify(object);
}

%PrepareFunctionForOptimization(TestLargeObjectElements);
TestLargeObjectElements();
TestLargeObjectElements();
%OptimizeFunctionOnNextCall(TestLargeObjectElements);
TestLargeObjectElements();
