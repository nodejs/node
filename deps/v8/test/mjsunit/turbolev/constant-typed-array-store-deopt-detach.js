// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev --no-always-turbofan

const ab = new ArrayBuffer(100);
var constTypedArray = new Uint16Array(ab);

function readTypedArray() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    assertEquals(i, constTypedArray[i]);
  }
}
%NeverOptimizeFunction(readTypedArray);

function foo(i, v) {
  constTypedArray[i] = v;
}

%PrepareFunctionForOptimization(foo);
foo(3, 4);
foo(10, 20);

assertEquals(4, constTypedArray[3]);
assertEquals(20, constTypedArray[10]);
%OptimizeFunctionOnNextCall(foo);

foo(13, 14);

assertOptimized(foo);
assertEquals(14, constTypedArray[13]);

// Detaching the buffer deopts.
%ArrayBufferDetach(ab);
assertUnoptimized(foo);
