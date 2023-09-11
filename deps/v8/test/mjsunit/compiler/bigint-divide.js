// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan


function OptimizeAndTest() {
  function fn(a, b) {
    return a / b;
  }

  %PrepareFunctionForOptimization(fn);
  assertEquals(0n, fn(3n, 4n));
  assertEquals(3n, fn(3n, 1n));
  assertEquals(4n, fn(30n, 7n));

  %OptimizeFunctionOnNextCall(fn);
  assertEquals(0n, fn(3n, 4n));
  assertEquals(3n, fn(3n, 1n));
  assertEquals(4n, fn(30n, 7n));
  assertOptimized(fn);

  assertEquals(5, fn(30, 6));
  assertUnoptimized(fn);
}

function OptimizeAndTestDivZero() {
  function fn(a, b) {
    return a / b;
  }

  %PrepareFunctionForOptimization(fn);
  assertEquals(0n, fn(3n, 4n));
  assertEquals(4n, fn(30n, 7n));

  %OptimizeFunctionOnNextCall(fn);
  assertEquals(4n, fn(30n, 7n));
  assertOptimized(fn);

  assertThrows(() => fn(3n, 0n), RangeError);
  assertUnoptimized(fn);
}

OptimizeAndTest();
OptimizeAndTestDivZero();
