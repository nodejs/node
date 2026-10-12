// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function testInBoundsStoreConstantIndex() {
  function f(a) {
    a[0] = 42;
    return a.length;
  }
  %PrepareFunctionForOptimization(f);
  let a = [1, 2, 3];
  assertEquals(3, f(a));
  assertEquals(3, f(a));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3, f(a));
  assertEquals(42, a[0]);
  assertOptimized(f);
}
testInBoundsStoreConstantIndex();

function testInBoundsStoreLoop() {
  function f(a) {
    for (let i = 0; i < a.length; i++) {
      a[i] = i * 2;
    }
    return a.length;
  }
  %PrepareFunctionForOptimization(f);
  let a = [1, 2, 3, 4, 5];
  assertEquals(5, f(a));
  assertEquals(5, f(a));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(5, f(a));
  assertEquals([0, 2, 4, 6, 8], a);
  assertOptimized(f);
}
testInBoundsStoreLoop();

function testGrowStoreLoop() {
  function f() {
    let a = [];
    for (let i = 0; i < 10; i++) {
      a[i] = i;
    }
    return a.length;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(10, f());
  assertEquals(10, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10, f());
  assertOptimized(f);
}
testGrowStoreLoop();
