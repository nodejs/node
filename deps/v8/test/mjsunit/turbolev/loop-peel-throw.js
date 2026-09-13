// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

function simple_throw(n) {
  let acc = 0;
  try {
    for (let i = 0; i < n; i++) {
      %AssertNotPeeled();
      if (n == 42) throw i;
      acc += i;
    }
  } catch (e) { print(acc); return -1; }
  return acc;
}
%PrepareFunctionForOptimization(simple_throw);
simple_throw(20);
simple_throw(42);
%OptimizeFunctionOnNextCall(simple_throw);
simple_throw(42);
assertOptimized(simple_throw);
