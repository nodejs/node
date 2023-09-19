// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function OptimizeAndTest() {
  function fn(a, b) {
    return a | b;
  }
  %PrepareFunctionForOptimization(fn);
  assertEquals(0b1110n, fn(0b1100n, 0b1010n));
  assertEquals(-0b0010n, fn(-0b100n, -0b110n));
  assertEquals(-0b0010n, fn(-0b100n, 0b1010n));
  assertEquals(-0b0010n, fn(0b1100n, -0b110n));
  assertEquals(-(2n ** 64n) + 1n, fn(-(2n ** 64n) + 1n, -(2n ** 64n)));

  %OptimizeFunctionOnNextCall(fn);
  fn(0b1100n, 0b1010n);
  assertOptimized(fn);

  assertEquals(0b1110n, fn(0b1100n, 0b1010n));
  assertEquals(-0b0010n, fn(-0b100n, -0b110n));
  assertEquals(-0b0010n, fn(-0b100n, 0b1010n));
  assertEquals(-0b0010n, fn(0b1100n, -0b110n));
  assertEquals(-(2n ** 64n) + 1n, fn(-(2n ** 64n) + 1n, -(2n ** 64n)));
  assertOptimized(fn);

  assertEquals(0b1110, fn(0b1100, 0b1010));
  assertUnoptimized(fn);
})();
