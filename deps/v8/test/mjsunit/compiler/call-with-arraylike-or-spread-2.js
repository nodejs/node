// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-optimize-apply --turbofan

// These tests do not work well if this script is run more than once; after a
// few runs the whole function is immediately compiled and assertions would
// fail. We prevent re-runs.
// Flags: --no-always-turbofan

// These tests do not work well if we flush the feedback vector, which causes
// deoptimization.
// Flags: --no-stress-flush-code --no-flush-bytecode

// Tests for optimization of CallWithSpread and CallWithArrayLike.
// This test is in a separate file because it invalidates protectors.

// Test with holey array with default values.
(function () {
  "use strict";

  // Setting value to be retrieved in place of hole.
  Array.prototype[2] = 'x';

  var sum_js_got_interpreted = true;
  function sum_js(a, b, c, d) {
    sum_js_got_interpreted = %IsBeingInterpreted();
    return a + b + c + d;
  }
  function foo(x, y) {
    return sum_js.apply(null, ["", x, ,y]);
  }

  %PrepareFunctionForOptimization(sum_js);
  %PrepareFunctionForOptimization(foo);
  assertEquals('AxB', foo('A', 'B'));
  assertTrue(sum_js_got_interpreted);

  // The protector should be invalidated, which prevents inlining.
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('AxB', foo('A', 'B'));
  assertTrue(sum_js_got_interpreted);
  assertOptimized(foo);
})();
