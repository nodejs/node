// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --array-destructure-bytecode

(function TestHoleWithIndexedPrototypeProperty() {
  Object.prototype[1] = 99;

  function f(a) {
    let [x, y, z] = a;
    return x + y + z;
  }

  let arr = [10, , 30];

  %PrepareFunctionForOptimization(f);
  assertEquals(139, f(arr));
  assertEquals(139, f(arr));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(139, f(arr));
  assertOptimized(f);
})();
