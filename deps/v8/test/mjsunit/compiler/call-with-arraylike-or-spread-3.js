// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-optimize-apply --opt

// These tests do not work well if this script is run more than once (e.g.
// --stress-opt); after a few runs the whole function is immediately compiled
// and assertions would fail. We prevent re-runs.
// Flags: --nostress-opt --no-always-opt

// These tests do not work well if we flush the feedback vector, which causes
// deoptimization.
// Flags: --no-stress-flush-code --no-flush-bytecode

// The test relies on optimizing/deoptimizing at predictable moments, so
// it's not suitable for deoptimization fuzzing.
// Flags: --deopt-every-n-times=0

// Tests for optimization of CallWithSpread and CallWithArrayLike.
// This test is in a separate file because it invalidates protectors.

// Test with array prototype modified after compilation.
(function () {
  "use strict";

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
  assertEquals('AundefinedB', foo('A', 'B'));
  assertTrue(sum_js_got_interpreted);

  %OptimizeFunctionOnNextCall(foo);
  assertEquals('AundefinedB', foo('A', 'B'));
  assertFalse(sum_js_got_interpreted);
  assertOptimized(foo);

  // Modify the array prototype, define a default value for element [1].
  Array.prototype[2] = 'x';
  assertUnoptimized(foo);

  // Now the call will not be inlined.
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('AxB', foo('A', 'B'));
  assertTrue(sum_js_got_interpreted);
  assertOptimized(foo);
})();
