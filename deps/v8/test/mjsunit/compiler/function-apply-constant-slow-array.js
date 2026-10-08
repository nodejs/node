// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbo-optimize-apply

(function() {
  const slow_args = [1, 2];
  %NormalizeElements(slow_args);

  function target(a, b) {
    return a + b;
  }
  function foo() {
    return target.apply(null, slow_args);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo());
  assertEquals(3, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo());
  // Because slow_args is not a fast array, TurboFan bails out during
  // ReduceCallOrConstructWithArrayLikeOrSpread without unrolling, falling back
  // to Builtin::kCallWithArrayLike.
  assertOptimized(foo);
})();
