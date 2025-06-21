// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function TestMultiply(a, b) {
  return a * b;
}

function OptimizeAndTest(fn) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(12n, fn(3n, 4n));
  assertEquals(30n, fn(5n, 6n));
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(56n, fn(7n, 8n));
  assertOptimized(fn);
  assertEquals(56, fn(7, 8));
  assertUnoptimized(fn);
}

OptimizeAndTest(TestMultiply);
