// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const ab = new ArrayBuffer(100);
var constTypedArray = new Int16Array(ab);

function fillTypedArray() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    constTypedArray[i] = -1-i;
  }
}
%NeverOptimizeFunction(fillTypedArray);

fillTypedArray();

function foo(i) {
  return constTypedArray[i];
}
%PrepareFunctionForOptimization(foo);

assertEquals(-4, foo(3));
assertEquals(-11, foo(10));

%OptimizeMaglevOnNextCall(foo);

function test() {
  for (let i = 0; i < constTypedArray.length; ++i) {
    assertEquals(-1-i, foo(i));
  }
}
%NeverOptimizeFunction(test);
test();

assertMaglevved(foo);

const dv = new DataView(ab);
dv.setInt16(0, 0x0080, true);

function loadBigEndianInt16(v) {
  return (v.getInt16(0, false) + 1) | 0;
}
%PrepareFunctionForOptimization(loadBigEndianInt16);
assertEquals(-32767, loadBigEndianInt16(dv));
%OptimizeMaglevOnNextCall(loadBigEndianInt16);
assertEquals(-32767, loadBigEndianInt16(dv));
assertMaglevved(loadBigEndianInt16);
