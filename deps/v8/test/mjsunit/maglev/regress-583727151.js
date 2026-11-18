// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestProtoTransitionNoneToDouble() {
  const p = {};
  function f(y) {
    return {__proto__: p, x: y};
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(1.5, f(1.5).x);
  assertEquals(2.5, f(2.5).x);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3.5, f(3.5).x);
})();

(function TestProtoTransitionGeneralizedTaggedBoilerplateToDouble() {
  const keep1 = {y: 1.5};
  const p = {};
  function g(v) {
    return {__proto__: p, y: v};
  }
  %PrepareFunctionForOptimization(g);
  assertEquals(1.5, g(1.5).y);
  assertEquals(2.5, g(2.5).y);
  const keep2 = {y: 'str'};
  assertEquals(1.5, keep1.y);
  assertEquals('str', keep2.y);
  %OptimizeFunctionOnNextCall(g);
  assertEquals(3.5, g(3.5).y);
})();
