// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --array-destructure-bytecode

(function TestCustomIteratorBailout() {
  let arr = [10, 20];
  arr[Symbol.iterator] = function*() {
    yield 99;
    yield 88;
  };

  function f(a) {
    let [x, y] = a;
    return x + y;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(187, f(arr));
  assertEquals(187, f(arr));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(187, f(arr));
  assertOptimized(f);
})();
