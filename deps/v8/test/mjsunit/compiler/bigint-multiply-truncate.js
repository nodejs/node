// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function TestMultiplyAndTruncate(a, b) {
  return BigInt.asIntN(3, a * b);
}

function OptimizeAndTest(fn) {
  let bi = 2n ** (2n ** 29n);
  // Before optimization, a BigIntTooBig exception is expected
  assertThrows(() => fn(bi + 3n, bi + 4n), RangeError);
  if (%Is64Bit()) {
    %PrepareFunctionForOptimization(fn);
    assertEquals(-4n, fn(3n, 4n));
    assertEquals(-2n, fn(5n, 6n));
    %OptimizeFunctionOnNextCall(fn);
    // After optimization, operands are truncated to Word64
    // before being multiplied. No exceptions should be thrown
    // and the correct result is expected.
    assertEquals(-4n, fn(bi + 3n, bi + 4n));
    assertOptimized(fn);
  }
}

OptimizeAndTest(TestMultiplyAndTruncate);
