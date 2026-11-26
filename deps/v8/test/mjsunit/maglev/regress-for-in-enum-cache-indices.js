// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --turbofan --no-concurrent-osr
// Flags: --no-stress-opt

// The enumerated keyed load {o[k]} never collects feedback, so it must use
// the enum cache indices instead of deopting for insufficient feedback.

(function TestForInWithoutFeedback() {
  function f(o) {
    for (let k in o) return o[k];
  }

  %PrepareFunctionForOptimization(f);
  f(undefined);
  %OptimizeFunctionOnNextCall(f);
  f(undefined);
  assertOptimized(f);
  // The for-in has no feedback yet.
  assertEquals(1, f({a: 1, b: 2}));
  assertOptimized(f);
})();

(function TestOsrIntoForIn() {
  function g(o) {
    let sum = 0;
    let iterations_in_osr_code = 0;
    for (let k in o) {
      %OptimizeOsr();
      sum += o[k];
      // Maglev code only tiers up to Turbofan OSR code with concurrent
      // recompilation, so count iterations in either tier.
      const status = %GetOptimizationStatus(g);
      if (status & (V8OptimizationStatus.kTopmostFrameIsMaglev |
                    V8OptimizationStatus.kTopmostFrameIsTurboFanned)) {
        iterations_in_osr_code++;
      }
    }
    return [sum, iterations_in_osr_code];
  }

  let o = {};
  for (let i = 0; i < 10; i++) o['p' + i] = i;
  %PrepareFunctionForOptimization(g);
  // Record for-in feedback, so that only OSR misses ForInPrepare.
  g({a: 0});
  let [sum, iterations_in_osr_code] = g(o);
  assertEquals(45, sum);
  assertTrue(iterations_in_osr_code > 0);
})();

// An enumerable accessor leaves the enum cache without indices, which must
// deopt cleanly. Object literals with accessors are in dictionary mode, so use
// defineProperty.
const object_with_accessor = {b: 2};
Object.defineProperty(
    object_with_accessor, 'a', {get() { return 1; }, enumerable: true});

(function TestForInWithoutFeedbackAccessor() {
  function f(o) {
    for (let k in o) return o[k];
  }

  %PrepareFunctionForOptimization(f);
  f(undefined);
  %OptimizeFunctionOnNextCall(f);
  f(undefined);
  assertOptimized(f);
  assertEquals(2, f(object_with_accessor));
})();

(function TestOsrIntoForInAccessor() {
  function g(o) {
    let sum = 0;
    for (let k in o) {
      %OptimizeOsr();
      sum += o[k];
    }
    return sum;
  }

  %PrepareFunctionForOptimization(g);
  // Compile OSR code with keys-and-indices feedback.
  assertEquals(3, g({a: 1, b: 2}));
  // Enter the cached OSR code with an accessor.
  assertEquals(3, g(object_with_accessor));
})();
