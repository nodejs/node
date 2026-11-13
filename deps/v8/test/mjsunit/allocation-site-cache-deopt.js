// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbofan
// Flags: --no-incremental-marking --no-stress-incremental-marking
// Flags: --no-stress-concurrent-allocation

// Test dependent code deoptimization when a cached array transitions.
(function TestDeoptOnCachedArrayTransition() {
  gc();
  function make() {
    return [1, 2];
  }
  %PrepareFunctionForOptimization(make);

  let a = make();
  assertTrue(%HasSmiElements(a));
  make();
  %OptimizeFunctionOnNextCall(make);
  let b = make();
  assertTrue(%HasSmiElements(b));
  if (!isNeverOptimizeLiteMode() && !isNeverOptimize()) {
    assertOptimized(make);
  }

  // Transitioning `a` (allocated before optimization) updates the
  // AllocationSite via AllocationSiteCache and triggers dependent code deopt.
  a[0] = 1.5;
  assertTrue(%HasDoubleElements(a));
  if (!isNeverOptimizeLiteMode() && !isNeverOptimize()) {
    assertUnoptimized(make);
  }

  let c = make();
  assertTrue(%HasDoubleElements(c));
})();
