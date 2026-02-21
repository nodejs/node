// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

(function() {
  function test(a, b) {
    return a === b;
  }

  %PrepareFunctionForOptimization(test);
  assertTrue(test(undefined, undefined));
  assertTrue(test(undefined, undefined));
  %OptimizeFunctionOnNextCall(test);
  assertTrue(test(undefined, undefined));
})();

(function() {
  function test(a, b) {
    return a === b;
  }

  %PrepareFunctionForOptimization(test);
  assertTrue(test(true, true));
  assertTrue(test(true, true));
  %OptimizeFunctionOnNextCall(test);
  assertFalse(test(true, 1));
})();

(function() {
  function test(a, b) {
      return a == b;
  }

  %PrepareFunctionForOptimization(test);
  assertTrue(test(true, true));
  assertTrue(test(true, true));
  %OptimizeFunctionOnNextCall(test);
  assertTrue(test(true, 1));
})();
