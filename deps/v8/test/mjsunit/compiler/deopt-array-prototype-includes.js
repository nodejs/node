// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

/* Test deopt behaviors when the prototype has elements */

// includes

(function() {
  const iarr = [0,1,2];
  const darr = [0.0, 2.0, 3.3];

  function includes(arr, val) {
    return arr.includes(val);
  }

  %PrepareFunctionForOptimization(includes);
  assertTrue(includes(iarr, 0)); assertTrue(includes(darr, 0));
  assertTrue(includes(iarr, 2)); assertTrue(includes(darr, 2));

  // JSCallReducer for includes not reduce because it only works with single map
  %OptimizeFunctionOnNextCall(includes);

  assertTrue(includes(iarr, 0));
  assertTrue(includes(darr, 0));
})();

(function() {
  const iarr = [0,1,2];

  function includes(arr, val) {
    return arr.includes(val);
  }

  %PrepareFunctionForOptimization(includes);
  assertTrue(includes(iarr, 0));
  assertTrue(includes(iarr, 2));

  %OptimizeFunctionOnNextCall(includes);

  assertTrue(includes(iarr, 0));

  const darr = [0.0, 2.0, 3.3];
  // deopt because of map change
  assertTrue(includes(darr, 0));
})();

(function() {
  const iarr = [,3];

  function includes(arr, val) {
    return arr.includes(val);
  }

  iarr.__proto__ = [2];

  // get feedback
  %PrepareFunctionForOptimization(includes);
  assertFalse(includes(iarr, 0));
  assertTrue(includes(iarr, 2));

  %OptimizeFunctionOnNextCall(includes);

  assertFalse(includes(iarr, 0));

  assertTrue(includes(iarr, 2));
})();

(function() {
  const iarr = [,3];

  function includes(arr, val) {
    return arr.includes(val);
  }

  %PrepareFunctionForOptimization(includes);
  assertFalse(includes(iarr, 2));
  assertTrue(includes(iarr, 3));

  %OptimizeFunctionOnNextCall(includes);
  assertFalse(includes(iarr, 2));

  // deopt because of map change
  iarr.__proto__ = [2];
  assertTrue(includes(iarr, 2));
})();

(function TestIncludesFromIndexTruncation() {
  function includes_in_range_neg(arr, val) { return arr.includes(val, -3); }
  function includes_in_range_pos(arr, val) { return arr.includes(val, 3); }

  function includes_extreme_neg(arr, val) { return arr.includes(val, -2147483649); }

  function includes_extreme_pos(arr, val) { return arr.includes(val, 2147483648); }
  function includes_extreme_huge(arr, val) { return arr.includes(val, 1e15); }

  const test_fns = [
    includes_in_range_neg,
    includes_in_range_pos,
    includes_extreme_neg,
    includes_extreme_pos,
    includes_extreme_huge,
  ];

  for (let f of test_fns) %PrepareFunctionForOptimization(f);

  assertEquals(true, includes_in_range_neg([1, 2, 3], 1));
  assertEquals(false, includes_in_range_pos([1, 2, 3], 1));

  assertEquals(true, includes_extreme_neg([1, 2, 3], 1));
  assertEquals(false, includes_extreme_pos([1, 2, 3], 1));
  assertEquals(false, includes_extreme_huge([1, 2, 3], 1));

  for (let f of test_fns) %OptimizeFunctionOnNextCall(f);

  assertEquals(true, includes_in_range_neg([1, 2, 3], 1));
  assertEquals(false, includes_in_range_pos([1, 2, 3], 1));

  assertEquals(true, includes_extreme_neg([1, 2, 3], 1));
  assertEquals(false, includes_extreme_pos([1, 2, 3], 1));
  assertEquals(false, includes_extreme_huge([1, 2, 3], 1));

  assertOptimized(includes_in_range_neg);
  assertOptimized(includes_in_range_pos);

  assertUnoptimized(includes_extreme_neg);
  assertUnoptimized(includes_extreme_pos);
  assertUnoptimized(includes_extreme_huge);
})();

// This pollutes the Array prototype, so we should not run more tests
// in the same environment after this.
(function () {
  var array = [,];

  function includes(val) {
    return array.includes(val);
  }

  %PrepareFunctionForOptimization(includes);
  includes(6); includes(6);

  %OptimizeFunctionOnNextCall(includes);
  assertFalse(includes(6));

  array.__proto__.push(6);
  // deopt because of no_elements_protector
  assertTrue(includes(6));
})();
