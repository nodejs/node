// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --deopt-every-n-times=0 --no-force-slow-path

(function TestSliceWithoutParams() {
  let array = [0, 1, 2];

  function f() {
    let array2 = array.slice();
    array2[1] = array2[0];
  }

  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f();

  // Assert that the function was not deoptimized.
  assertOptimized(f);
})();

(function TestSliceWithStartZero() {
  let array = [0, 1, 2];

  function f() {
    let array2 = array.slice(0);
    array2[1] = array2[0];
  }

  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f();

  // Assert that the function was not deoptimized.
  assertOptimized(f);
})();

(function TestSliceWithStartNonZero() {
  let array = [0, 1, 2];

  function f() {
    let array2 = array.slice(1);
    array2[1] = array2[0];
  }

  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f();

  // Assert that the function was not deoptimized.
  assertOptimized(f);
})();

(function TestSliceWithStartZeroEndNonUndefined() {
  let array = [0, 1, 2];

  function f() {
    let array2 = array.slice(0, 1);
    array2[1] = array2[0];
  }

  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f();

  // Assert that the function was not deoptimized.
  assertOptimized(f);
})();
