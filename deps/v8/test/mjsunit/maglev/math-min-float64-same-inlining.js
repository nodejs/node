// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function testDouble() {
  let v = 5.5;
  return Math.min(v, v);
}
%PrepareFunctionForOptimization(testDouble);
testDouble();

%OptimizeMaglevOnNextCall(testDouble);

assertEquals(5.5, testDouble());
assertTrue(isMaglevved(testDouble));

function testObject() {
  let v = {valueOf: () => {return 1.1;}}
  return Math.min(v, v);
}
%PrepareFunctionForOptimization(testObject);
testObject();

%OptimizeMaglevOnNextCall(testObject);

assertEquals(1.1, testObject());
assertFalse(isMaglevved(testObject));
