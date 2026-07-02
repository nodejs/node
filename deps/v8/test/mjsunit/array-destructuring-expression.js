// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --array-destructure-bytecode

(function TestDestructuringAsValueExpression() {
  let a, b;
  let arr = [10, 20];
  let res = ([a, b] = arr);
  assertEquals(arr, res);
  assertEquals(10, a);
  assertEquals(20, b);
})();

(function TestDestructuringInReturnStatement() {
  function f(arr) {
    let x, y;
    return ([x, y] = arr);
  }

  %PrepareFunctionForOptimization(f);
  let arr = [30, 40];
  assertEquals(arr, f(arr));
  assertEquals(arr, f(arr));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(arr, f(arr));
  assertOptimized(f);
})();

(function TestChainedDestructuringAssignments() {
  function f(arr) {
    let a, b, c, d;
    let res = ([a, b] = ([c, d] = arr));
    assertEquals(arr, res);
    return a + b + c + d;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(220, f([50, 60]));
  assertEquals(220, f([50, 60]));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(220, f([50, 60]));
  assertOptimized(f);
})();

(function TestDestructuringWithDefaultValuesInExpression() {
  function f(arr) {
    let x, y;
    return ([x = 100, y = 200] = arr);
  }

  %PrepareFunctionForOptimization(f);
  let arr1 = [5, undefined];
  assertEquals(arr1, f(arr1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(arr1, f(arr1));
  assertOptimized(f);
})();

(function TestDestructuringWithHoles() {
  function f(arr) {
    let x, z;
    return ([x, , z] = arr);
  }

  %PrepareFunctionForOptimization(f);
  let arr = [10, 20, 30];
  assertEquals(arr, f(arr));
  assertEquals(arr, f(arr));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(arr, f(arr));
  assertOptimized(f);
})();
