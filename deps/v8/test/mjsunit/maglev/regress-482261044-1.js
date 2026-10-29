// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling

function foo(n) {
  let sum = 0;
  for (let i = 0; i < 10; i++) {
    if (i == n) {
      sum = 0x4e000000;
    }
    sum + 2;
  }
  return sum;
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(1000));

%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo(1000));
assertEquals(0x4e000000, foo(8));
assertEquals(0x4e000000, foo(9));
