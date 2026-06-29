// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestMaterializeTargetOfInterpretedFrame() {
  function f(x) {
    function g() {
      %_DeoptimizeNow();
      return x + 1;
    }
    return g();
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(24, f(23));
  assertEquals(43, f(42));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(66, f(65));
})();

(function TestMaterializeTargetOfArgumentsAdaptorFrame() {
  function f(x) {
    function g(a, b, c) {
      %_DeoptimizeNow();
      return x + 1;
    }
    return g();
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(24, f(23));
  assertEquals(43, f(42));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(66, f(65));
})();

(function TestMaterializeTargetOfConstructStubFrame() {
  function f(x) {
    function g() {
      %_DeoptimizeNow();
      this.val = x + 1;
    }
    return new g();
  }
  %PrepareFunctionForOptimization(f);
  assertEquals({ val: 24 }, f(23));
  assertEquals({ val: 43 }, f(42));
  %OptimizeFunctionOnNextCall(f);
  assertEquals({ val: 66 }, f(65));
})();
