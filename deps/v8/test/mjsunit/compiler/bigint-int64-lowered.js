// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function TestAsIntN() {
  assertEquals(0n, BigInt.asIntN(64, 0n));
  assertEquals(0n, BigInt.asIntN(8, 0n));
  assertEquals(0n, BigInt.asIntN(1, 0n));
  assertEquals(0n, BigInt.asIntN(0, 0n));
  assertEquals(0n, BigInt.asIntN(100, 0n));

  assertEquals(123n, BigInt.asIntN(64, 123n));
  assertEquals(123n, BigInt.asIntN(32, 123n));
  assertEquals(123n, BigInt.asIntN(8, 123n));
  assertEquals(-5n, BigInt.asIntN(6, 123n));
  assertEquals(-5n, BigInt.asIntN(5, 123n));
  assertEquals(-5n, BigInt.asIntN(4, 123n));
  assertEquals(-1n, BigInt.asIntN(1, 123n));
  assertEquals(0n, BigInt.asIntN(0, 123n));
  assertEquals(123n, BigInt.asIntN(72, 123n));

  assertEquals(-123n, BigInt.asIntN(64, -123n));
  assertEquals(-123n, BigInt.asIntN(32, -123n));
  assertEquals(-123n, BigInt.asIntN(8, -123n));
  assertEquals(5n, BigInt.asIntN(6, -123n));
  assertEquals(5n, BigInt.asIntN(5, -123n));
  assertEquals(5n, BigInt.asIntN(4, -123n));
  assertEquals(-1n, BigInt.asIntN(1, -123n));
  assertEquals(0n, BigInt.asIntN(0, -123n));
  assertEquals(-123n, BigInt.asIntN(72, -123n));
}

function TestInt64LoweredOperations() {
  assertEquals(0n, BigInt.asIntN(64, -0n));
  assertEquals(0n, BigInt.asIntN(64, 15n + -15n));
  assertEquals(0n, BigInt.asIntN(64, 0n + 0n));
  assertEquals(14n, BigInt.asIntN(32, 8n + 6n));
  assertEquals(-211n, BigInt.asIntN(10, 1013n + -200n));
  assertEquals(-1n, BigInt.asIntN(4, -319n + 302n));
  assertEquals(32n, BigInt.asIntN(64, (2n ** 100n + 64n) - 32n));
  assertEquals(-32n, BigInt.asIntN(64, 32n - (2n ** 100n + 64n)));
  assertEquals(-5n, BigInt.asIntN(4, 800n - 789n));
  assertEquals(5n, BigInt.asIntN(4, 789n - 800n));

  for (let i = 0; i < 2; ++i) {
    let x = 32n; // x = 32n
    if (i === 1) {
      x = BigInt.asIntN(64, x + 3n); // x = 35n
      const y = x + -8n + x; // x = 35n, y = 62n
      x = BigInt.asIntN(6, y + x); // x = -31n, y = 62n
      x = -9n + y - x; // x = 84n
      x = BigInt.asIntN(10000 * i, x); // x = 84n
    } else {
      x = x + 400n; // x = 432n
      x = 176n + BigInt.asIntN(8, 500n) + x; // x = 596n
    }
    assertEquals(84n, BigInt.asIntN(8, x));
  }

  let x = 7n;
  for (let i = 0; i < 10; ++i) {
    x = x + 5n;
  }
  assertEquals(-7n, BigInt.asIntN(6, x));

  let y = 7n;
  for(let i = 0; i < 10; ++i) {
    y = BigInt.asIntN(4, y + 16n);
  }
  assertEquals(7n, y);
}

function OptimizeAndTest(fn) {
  %PrepareFunctionForOptimization(fn);
  %PrepareFunctionForOptimization(assertEquals);
  %PrepareFunctionForOptimization(deepEquals);
  fn();
  fn();
  %OptimizeFunctionOnNextCall(fn);
  fn();
  assertOptimized(fn);
  fn();
}

OptimizeAndTest(TestAsIntN);
OptimizeAndTest(TestInt64LoweredOperations);
