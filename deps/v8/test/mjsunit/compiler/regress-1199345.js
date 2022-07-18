// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan


(function() {
  function foo(a) {
    var x = -0;
    if (a) {
      x = 0;
    }
    return x + (x - 0);
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-0, foo(false));
})();


// The following test already passed before the bugfix.

(function() {
  function foo(a) {
    var x = 0;
    var y = -0;
    if (a == 42) x = 2**32 - 1;
    if (a == 0) {
      x = 0
      y = 1;
    }
    if (a == 2) x = -0;
    return x + y;
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  // We don't automatically deopt for y = -0 because (x + -0) can only become -0
  // for x = -0.
  assertOptimized(foo);
  assertEquals(-0, foo(2));
  assertUnoptimized(foo);
})();
