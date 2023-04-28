// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function OptimizeAndTestAsUintN() {
  function f(x) {
    // Will be lowered to Int64Constant(-1) and stored as an immediate.
    let y = BigInt.asUintN(64, -1n);
    try {
      return x + y;
    } catch(_) {
      return y;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(2n ** 64n, f(1n));
  assertEquals(2n ** 64n + 1n, f(2n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(2n ** 64n, f(1n));
  assertOptimized(f);
  // Should be rematerialized to 2n ** 64n - 1n in code generation.
  assertEquals(2n ** 64n - 1n, f(0));
  if (%Is64Bit()) {
    assertUnoptimized(f);
  }
})();

(function OptimizeAndTestAsUintN() {
  function f(x) {
    // Will be lowered to Int64Sub because exponentiation is not truncated and
    // stored in a register.
    let y = BigInt.asUintN(64, -(2n ** 0n));
    try {
      return x + y;
    } catch(_) {
      return y;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(2n ** 64n, f(1n));
  assertEquals(2n ** 64n + 1n, f(2n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(2n ** 64n, f(1n));
  assertOptimized(f);
  // Should be rematerialized to 2n ** 64n - 1n in deoptimization.
  assertEquals(2n ** 64n - 1n, f(0));
  if (%Is64Bit()) {
    assertUnoptimized(f);
  }
})();

(function OptimizeAndTestAsUintN() {
  function f(x) {
    // Will be lowered to Int64Sub because exponentiation is not truncated and
    // stored in a stack slot.
    let y = BigInt.asUintN(64, -(2n ** 0n));
    try {
      // The recursion is used to make sure `y` is stored on the stack.
      return (x < 3n) ? (x + y) : f(x - 1n);
    } catch(_) {
      return y;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(2n ** 64n, f(1n));
  assertEquals(2n ** 64n + 1n, f(2n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(2n ** 64n, f(1n));
  assertOptimized(f);
  // Should be rematerialized to 2n ** 64n - 1n in deoptimization.
  assertEquals(2n ** 64n - 1n, f(0));
  if (%Is64Bit()) {
    assertUnoptimized(f);
  }
})();

(function OptimizeAndTestAsIntN() {
  function f(x) {
    // Will be lowered to Int64Constant(-1) and stored as an immediate.
    let y = BigInt.asIntN(64, -1n);
    try {
      return x + y;
    } catch (_) {
      return y;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(0n, f(1n));
  assertEquals(1n, f(2n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0n, f(1n));
  assertOptimized(f);
  // Should be rematerialized to -1n in code generation.
  assertEquals(-1n, f(0));
  if (%Is64Bit()) {
    assertUnoptimized(f);
  }
})();

(function OptimizeAndTestAsIntN() {
  function f(x) {
    // Will be lowered to Int64Sub because exponentiation is not truncated and
    // stored in a register.
    let y = BigInt.asIntN(64, -(2n ** 0n));
    try {
      return x + y;
    } catch(_) {
      return y;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(0n, f(1n));
  assertEquals(1n, f(2n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0n, f(1n));
  assertOptimized(f);
  // Should be rematerialized to -1n in deoptimization.
  assertEquals(-1n, f(0));
  if (%Is64Bit()) {
    assertUnoptimized(f);
  }
})();

(function OptimizeAndTestAsIntN() {

  function f(x) {
    // Will be lowered to Int64Sub because exponentiation is not truncated and
    // stored in a stack slot.
    let y = BigInt.asIntN(64, -(2n ** 0n));
    try {
      // The recursion is used to make sure `y` is stored on the stack.
      return (x < 3n) ? (x + y) : f(x - 1n);
    } catch(_) {
      return y;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(0n, f(1n));
  assertEquals(1n, f(2n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0n, f(1n));
  assertOptimized(f);
  // Should be rematerialized to -1n in deoptimization.
  assertEquals(-1n, f(0));
  if (%Is64Bit()) {
    assertUnoptimized(f);
  }
})();
