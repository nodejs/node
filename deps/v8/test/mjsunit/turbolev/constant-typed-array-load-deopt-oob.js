// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev --no-always-turbofan

const ab = new ArrayBuffer(100);
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

%OptimizeFunctionOnNextCall(foo);

assertEquals(3, foo(3));
assertEquals(10, foo(10));

assertOptimized(foo);

// Going out of bounds deopts.
assertEquals(undefined, foo(1000));
assertUnoptimized(foo);
