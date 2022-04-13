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
// this is not suitable for deoptimization fuzzing.
// Flags: --deopt-every-n-times=0 --interrupt-budget=1024

// Tests for optimization of CallWithSpread and CallWithArrayLike.

// Test deopt when array map changes.
(function () {
  "use strict";
  var sum_js_got_interpreted = true;
  function sum_js(a, b, c) {
    sum_js_got_interpreted = %IsBeingInterpreted();
    return a + b + c;
  }
  function foo(x, y, z, str) {
    let v = [x, y, z];
    if (str) {
      v[0] = str;
    }
    return sum_js.apply(null, v);
  }

  %PrepareFunctionForOptimization(sum_js);
  for (let i = 0; i < 5; i++) {
    %PrepareFunctionForOptimization(foo);
    assertEquals(78, foo(26, 6, 46, null));
    assertTrue(sum_js_got_interpreted);

    // Compile function foo; inlines 'sum_js' into 'foo'.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(78, foo(26, 6, 46, null));
    assertOptimized(foo);
    %PrepareFunctionForOptimization(foo);

    if (i < 3) {
      assertFalse(sum_js_got_interpreted);
    } else {
      // i: 3: Speculation mode prevents optimization of sum_js.apply() call.
      assertTrue(sum_js_got_interpreted);
    }

    // This should deoptimize:
    // i: 0: Deopt soft: insufficient type feedback for generic keyed access.
    // i: 1,2: Deopt eager: wrong map.
    // i: 3: Won't deopt anymore.
    assertEquals("v8646", foo(26, 6, 46, "v8"));
    assertTrue(sum_js_got_interpreted);

    if (i < 3) {
      assertUnoptimized(foo);
    } else {
      assertOptimized(foo);
      %PrepareFunctionForOptimization(foo);
    }
  }
})();
