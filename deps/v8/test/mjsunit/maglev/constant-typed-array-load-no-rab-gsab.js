// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --no-optimize-maglev-optimizes-to-turbofan

const ab = new ArrayBuffer(100, {maxByteLength: 300});
var constTypedArray = new Uint16Array(ab);

function fillTypedArray() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    constTypedArray[i] = i;
  }
}
%NeverOptimizeFunction(fillTypedArray);

fillTypedArray();

function foo(i) {
  return constTypedArray[i];
}
%PrepareFunctionForOptimization(foo);

assertEquals(3, foo(3));
assertEquals(10, foo(10));

%OptimizeMaglevOnNextCall(foo);

assertEquals(3, foo(3));
assertEquals(10, foo(10));

assertTrue(isMaglevved(foo));

// We don't optimize loading from the constant typed array, so we then also
// don't deopt if we read out of bounds, the array buffer is detached, etc.
assertEquals(undefined, foo(1000));
assertTrue(isMaglevved(foo));

ab.resize(50);
assertEquals(undefined, foo(30));
assertTrue(isMaglevved(foo));

%ArrayBufferDetach(ab);
assertTrue(isMaglevved(foo));
