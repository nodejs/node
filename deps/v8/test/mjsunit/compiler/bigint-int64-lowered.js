// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestAsUintN() {
  assertEquals(0n, BigInt.asUintN(64, 0n));
  assertEquals(0n, BigInt.asUintN(8, 0n));
  assertEquals(0n, BigInt.asUintN(1, 0n));
  assertEquals(0n, BigInt.asUintN(0, 0n));
  assertEquals(0n, BigInt.asUintN(100, 0n));

  assertEquals(123n, BigInt.asUintN(64, 123n));
  assertEquals(123n, BigInt.asUintN(32, 123n));
  assertEquals(123n, BigInt.asUintN(8, 123n));
  assertEquals(59n, BigInt.asUintN(6, 123n));
  assertEquals(27n, BigInt.asUintN(5, 123n));
  assertEquals(11n, BigInt.asUintN(4, 123n));
  assertEquals(1n, BigInt.asUintN(1, 123n));
  assertEquals(0n, BigInt.asUintN(0, 123n));
  assertEquals(123n, BigInt.asUintN(72, 123n));

  assertEquals(BigInt("0xFFFFFFFFFFFFFF85"), BigInt.asUintN(64, -123n));
  assertEquals(BigInt("0xFFFFFF85"), BigInt.asUintN(32, -123n));
  assertEquals(BigInt("0x85"), BigInt.asUintN(8, -123n));
  assertEquals(5n, BigInt.asUintN(6, -123n));
  assertEquals(5n, BigInt.asUintN(5, -123n));
  assertEquals(5n, BigInt.asUintN(4, -123n));
  assertEquals(1n, BigInt.asUintN(1, -123n));
  assertEquals(0n, BigInt.asUintN(0, -123n));
  assertEquals(BigInt("0xFFFFFFFFFFFFFFFF85"), BigInt.asUintN(72, -123n));
}

function TestInt64LoweredOperations() {
  assertEquals(0n, BigInt.asUintN(64, -0n));
  assertEquals(0n, BigInt.asUintN(64, 15n + -15n));
  assertEquals(0n, BigInt.asUintN(64, 0n + 0n));
  assertEquals(14n, BigInt.asUintN(32, 8n + 6n));
  assertEquals(813n, BigInt.asUintN(10, 1013n + -200n));
  assertEquals(15n, BigInt.asUintN(4, -319n + 302n));

  for (let i = 0; i < 2; ++i) {
    let x = 32n; // x = 32n
    if (i === 1) {
      x = BigInt.asUintN(64, x + 3n); // x = 35n
      const y = x + -8n + x; // x = 35n, y = 62n
      x = BigInt.asUintN(6, y + x); // x = 33n, y = 62n
      x = -9n + y + -x; // x = 20n
      x = BigInt.asUintN(10000 * i, x); // x = 20n
    } else {
      x = x + 400n; // x = 432n
      x = -144n + BigInt.asUintN(8, 500n) + x; // x = 532n
    }
    assertEquals(20n, BigInt.asUintN(8, x));
  }

  let x = 7n;
  for (let i = 0; i < 10; ++i) {
    x = x + 5n;
  }
  assertEquals(57n, BigInt.asUintN(8, x));

  let y = 7n;
  for(let i = 0; i < 10; ++i) {
    y = BigInt.asUintN(4, y + 16n);
  }
  assertEquals(7n, y);
}

function OptimizeAndTest(fn) {
  %PrepareFunctionForOptimization(fn);
  fn();
  fn();
  %OptimizeFunctionOnNextCall(fn);
  fn();
  assertOptimized(fn);
  fn();
}

OptimizeAndTest(TestAsUintN);
OptimizeAndTest(TestInt64LoweredOperations);
