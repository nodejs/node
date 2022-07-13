// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestNegate() {
  assertEquals(0n, -0n);

  const x = 15n;
  assertEquals(-15n, -x);
  assertEquals(15n, - -x);
  assertEquals(30n, -(-x + -x));
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

OptimizeAndTest(TestNegate);
