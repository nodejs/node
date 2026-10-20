// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert-types --maglev

// Create a HeapNumber with value 1.0.
let arr = [undefined];
arr[0] = 1;
let v7 = arr[0];

function foo(b, storeHere) {
  const phi = b ? v7 : 5.6;
  0 % phi;
  storeHere.p = phi;
}
%PrepareFunctionForOptimization(foo);
const obj = { p: {} };
foo(0, obj);
%OptimizeMaglevOnNextCall(foo);
foo(1, obj);
