// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

const ab = new ArrayBuffer(100);
var constTypedArray = new Float32Array(ab);

function fillTypedArray() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    constTypedArray[i] = i + 0.5;
  }
}
%NeverOptimizeFunction(fillTypedArray);

fillTypedArray();

function foo(i) {
  return constTypedArray[i];
}
%PrepareFunctionForOptimization(foo);

assertEquals(3.5, foo(3));
assertEquals(10.5, foo(10));

%OptimizeMaglevOnNextCall(foo);

function test() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    assertEquals(i + 0.5, foo(i));
  }
}
%NeverOptimizeFunction(test);
test();

assertTrue(isMaglevved(foo));
