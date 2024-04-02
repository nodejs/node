// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, b, c) {
  let x = BigInt.asUintN(0, a + b);
  return BigInt.asUintN(64, x + c);
}

%PrepareFunctionForOptimization(foo);
assertEquals(1n, foo(9n, 2n, 1n));
%OptimizeFunctionOnNextCall(foo);
assertEquals(1n, foo(9n, 2n, 1n));
