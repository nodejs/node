// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt

// Test that String.fromCodePoint() properly identifies zeros.
(function() {
  function foo(x) {
    return String.fromCodePoint(x);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals("\u0000", foo(0));
  assertEquals("\u0000", foo(-0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("\u0000", foo(0));
  assertEquals("\u0000", foo(-0));
  assertOptimized(foo);

  // Prepare foo to be re-optimized, ensuring it's bytecode / feedback vector
  // doesn't get flushed after deoptimization.
  %PrepareFunctionForOptimization(foo);

  // Now passing anything outside the valid code point
  // range should invalidate the optimized code.
  assertThrows(_ => foo(-1));
  assertUnoptimized(foo);

  // And TurboFan should not inline the builtin anymore
  // from now on (aka no deoptimization loop).
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("\u0000", foo(0));
  assertEquals("\u0000", foo(-0));
  assertThrows(_ => foo(-1));
  assertOptimized(foo);
})();
