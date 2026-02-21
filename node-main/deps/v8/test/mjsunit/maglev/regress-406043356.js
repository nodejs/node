// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

(function() {
  function foo(a) {
    v = a % a;
    return 1 / v;
  }
  %PrepareFunctionForOptimization(foo);
  // Ensures global {v} has constant cell -0.
  foo(-1);
  foo(-1);
  // Ensures we deopt when comparing 0 to -0.
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(Infinity, foo(1));
  assertFalse(isOptimized(foo));
})();

(function() {
  function foo(a) {
    w = a % a;
    return 1 / w;
  }
  %PrepareFunctionForOptimization(foo);
  // Ensures global {w} has constant cell 0.
  foo(1);
  foo(1);
  // Ensures we deopt when comparing 0 to -0.
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-Infinity, foo(-1));
  assertFalse(isOptimized(foo));
})();

(function() {
  let ab = new ArrayBuffer(16);
  let int32view = new Int32Array(ab);
  int32view[1] = 0x7FFF_FFFF;
  int32view[0] = 0x0000_0000;
  int32view[3] = 0x7FFF_FFFF;
  int32view[2] = 0x0000_0001;
  let float64view = new Float64Array(ab);
  function foo(a) {
    vvv = a;
  }
  %PrepareFunctionForOptimization(foo);
  foo(float64view[0]);
  foo(float64view[0]);

  %OptimizeFunctionOnNextCall(foo);
  // Ensures we don't deopt if we use a different NaN bit
  // representation.
  foo(float64view[1]);
  assertTrue(isOptimized(foo));
})();
