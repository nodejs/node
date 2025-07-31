// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev --no-always-turbofan

const ab = new ArrayBuffer(100);
var constTypedArray = new Float32Array(ab);

function readTypedArray() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    assertEquals(i + 0.5, constTypedArray[i]);
  }
}
%NeverOptimizeFunction(readTypedArray);

function foo(i, v) {
  constTypedArray[i] = v;
}

%PrepareFunctionForOptimization(foo);
foo(3, 4.5);
foo(10, 20.5);

assertEquals(4.5, constTypedArray[3]);
assertEquals(20.5, constTypedArray[10]);

%OptimizeFunctionOnNextCall(foo);

function test() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    foo(i, i + 0.5);
  }
  readTypedArray();
}
%NeverOptimizeFunction(test);
test();

assertOptimized(foo);
