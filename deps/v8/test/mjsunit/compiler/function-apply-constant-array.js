// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbo-optimize-apply

(function() {
  const const_args = [1, 2];
  function target(a, b) {
    return a + b;
  }
  function foo() {
    return target.apply(null, const_args);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo());
  assertEquals(3, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo());
  assertOptimized(foo);

  // Mutating the length of the constant array should trigger a deopt check.
  const_args.push(3);
  assertEquals(3, foo());
  assertUnoptimized(foo);
})();
