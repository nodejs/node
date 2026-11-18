// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var g_bool = true;
g_bool = false;

(function() {
  function noop(o) { return 0; }
  function readBool(o) {
    g_bool = o;
    return g_bool ? 1 : 0;
  }
  function dispatch(fn, o) { return fn(o); }

  %PrepareFunctionForOptimization(readBool);
  %PrepareFunctionForOptimization(dispatch);
  dispatch(noop, 0);
  dispatch(readBool, true);
  dispatch(readBool, false);

  // Not in read-only space.
  const str = "0123456789abcdef" + Math.random();

  function trigger(a, flag, use_read_bool) {
    const v = a | 0;
    // Only folded to 0 after simplified lowering, at which point the ToBoolean
    // in the inlined readBool sees {str} as a constant input.
    const zero = (v - v) | 0;
    const b = flag ? true : false;
    return dispatch(use_read_bool ? readBool : noop, zero ? b : str);
  }

  %PrepareFunctionForOptimization(trigger);
  for (let i = 0; i < 5; i++) trigger(i, (i & 1) === 0, false);
  %OptimizeFunctionOnNextCall(trigger, "concurrent");
  assertEquals(0, trigger(0, false, false));
  %WaitForBackgroundOptimization();
  assertEquals(0, trigger(0, false, false));
})();
