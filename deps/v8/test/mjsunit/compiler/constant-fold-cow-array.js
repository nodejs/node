// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt --opt

// Check that we properly deoptimize TurboFan'ed code when we constant-fold
// elements from a COW array and we change the length of the array.
(function() {
  const a = [1, 2, 3];
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a.length = 1;
  assertEquals(1, foo());
  assertUnoptimized(foo);
})();

// Check that we properly deoptimize TurboFan'ed code when we constant-fold
// elements from a COW array and we change the element of the array.
(function() {
  const a = [1, 2, 3];
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(42, foo());
  assertUnoptimized(foo);
})();

// Packed
// Non-extensible
(function() {
  const a = Object.preventExtensions([1, 2, '3']);
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(42, foo());
})();

// Sealed
(function() {
  const a = Object.seal([1, 2, '3']);
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(42, foo());
})();

// Frozen
(function() {
  const a = Object.freeze([1, 2, '3']);
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(1, foo());
})();

// Holey
// Non-extensible
(function() {
  const a = Object.preventExtensions([1, 2, , '3']);
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(42, foo());
})();

// Sealed
(function() {
  const a = Object.seal([1, 2, , '3']);
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(42, foo());
})();

// Frozen
(function() {
  const a = Object.freeze([1, 2, , '3']);
  const foo = () => a[0];
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
  assertOptimized(foo);
  a[0] = 42;
  assertEquals(1, foo());
})();
