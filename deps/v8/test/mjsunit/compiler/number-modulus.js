// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt


// Test that NumberModulus passes kIdentifiesZero to the
// left hand side input when the result doesn't care about
// 0 vs -0, even when the inputs are outside Signed32.
(function() {
  function foo(x) {
    return (x * -2) % (2 ** 32) === 0;
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(2));
  assertFalse(foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(2));
  assertFalse(foo(1));

  // Now `foo` should stay optimized even if `x * -2` would
  // produce -0, aka when we pass a zero value for `x`.
  assertTrue(foo(0));
  assertOptimized(foo);
})();
