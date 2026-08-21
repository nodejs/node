// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --stress-concurrent-inlining --stress-concurrent-inlining-attach-code
// Flags: --no-maglev-overwrite-budget

// Regression test for crbug.com/503098806: Turboshaft graph builder crash
// (dominating_frame_state.valid()) in the inlined Array.prototype.sort
// reduction when the comparefn contains a try-catch.
//
// This crash is timing-dependent (concurrent compilation race).  Increase
// probability of hitting the race with multiple optimization passes.

(function () {
  let throwing = false;
  function cmp(a, b) {
    try {
      if (throwing) throw 'caught';
      return a - b;
    } catch (e) {}
  }
  function f() {
    return [4, 13].sort(cmp);
  }
  throwing = true;
  throwing = false;

  // Multiple optimization attempts to increase race probability.
  for (let i = 0; i < 5; i++) {
    %PrepareFunctionForOptimization(f);
    f();
    %OptimizeFunctionOnNextCall(f);
    f();
  }
})();
