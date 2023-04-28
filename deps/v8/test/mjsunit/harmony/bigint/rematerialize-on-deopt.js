// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

{
  function test(a, b, c) {
    let x = BigInt.asUintN(64, a + b);
    console.log(x);
    try {
      return BigInt.asUintN(64, x + c);
    } catch(_) {
      return x;
    }
  }

  %PrepareFunctionForOptimization(test);
  test(3n, 4n, 5n);
  test(6n, 7n, 8n);
  test(9n, 2n, 1n);
  %OptimizeFunctionOnNextCall(test);
  test(1n, 2n, 3n);
  test(3n, 2n, 1n);

  assertEquals(6n, test(1n, 3n, 2n));
  assertEquals(5n, test(2n, 3n, 2));
}
