// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --no-optimize-maglev-optimizes-to-turbofan

const ab = new ArrayBuffer(100, {maxByteLength: 300});
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
%OptimizeMaglevOnNextCall(foo);

foo(13, 14);

assertTrue(isMaglevved(foo));
assertEquals(14, constTypedArray[13]);

// We don't optimize storing into the constant typed array, so we then also
// don't deopt if we store out of bounds, the array buffer is detached, etc.
foo(1000, 0);
assertTrue(isMaglevved(foo));

ab.resize(50);
foo(30, 0);
assertTrue(isMaglevved(foo));

%ArrayBufferDetach(ab);
assertTrue(isMaglevved(foo));
