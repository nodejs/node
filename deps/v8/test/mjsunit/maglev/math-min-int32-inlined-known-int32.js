// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function min2(a, b) {
  return Math.min(a, b);
}
%PrepareFunctionForOptimization(min2);

function foo(arr) {
  // The parameters are not const, but known to be Int32.
  return min2(arr[0], arr[1]);
}
%PrepareFunctionForOptimization(foo);

let ta = new Int32Array(10);
ta[0] = 12;
ta[1] = 11;
foo(ta);

%OptimizeMaglevOnNextCall(foo);
assertEquals(11, foo(ta));
