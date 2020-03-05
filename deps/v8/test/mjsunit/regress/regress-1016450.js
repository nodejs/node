// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

let g = 0;

function f(x) {
  let y = BigInt.asUintN(64, 15n);
  // Introduce a side effect to force the construction of a FrameState that
  // captures the value of y.
  g = 42;
  try {
    return x + y;
  } catch(_) {
    return y;
  }
}


%PrepareFunctionForOptimization(f);
assertEquals(16n, f(1n));
assertEquals(17n, f(2n));
%OptimizeFunctionOnNextCall(f);
assertEquals(16n, f(1n));
assertOptimized(f);
assertEquals(15n, f(0));
assertUnoptimized(f);
