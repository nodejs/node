// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Transitioning store to a const double field from a holey double array.
(function TestConstTransitioningDoubleStore() {
  function F(a, i) {
    this.x = a[i];
  }

  const arr = [1.5, , 2.5];
  %PrepareFunctionForOptimization(F);
  assertEquals(1.5, new F(arr, 0).x);
  assertEquals(2.5, new F(arr, 2).x);

  %OptimizeMaglevOnNextCall(F);
  assertEquals(1.5, new F(arr, 0).x);
  assertEquals(undefined, new F(arr, 1).x);
})();

// Transitioning store to a mutable double field from a holey double array.
(function TestMutableTransitioningDoubleStore() {
  function F(a, i) {
    this.x = a[i];
  }

  const arr = [1.5, , 2.5];
  %PrepareFunctionForOptimization(F);
  const o = new F(arr, 0);
  o.x = 3.5;
  assertEquals(2.5, new F(arr, 2).x);

  %OptimizeMaglevOnNextCall(F);
  assertEquals(1.5, new F(arr, 0).x);
  assertEquals(undefined, new F(arr, 1).x);
})();
