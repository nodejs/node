// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

/* Test deopt behaviors when the prototype has elements */

// indexOf

(function() {
  const iarr = [0,1,2];
  const darr = [0.0, 2.0, 3.3];

  function indexOf(arr, val) {
    return arr.indexOf(val);
  }

  %PrepareFunctionForOptimization(indexOf);
  assertEquals(0, indexOf(iarr, 0));
  assertEquals(0, indexOf(darr, 0));
  assertEquals(2, indexOf(iarr, 2));
  assertEquals(1, indexOf(darr, 2));

  // JSCallReducer for indexOf will not reduce
  // because it only works with single map
  %OptimizeFunctionOnNextCall(indexOf);

  assertEquals(0, indexOf(iarr, 0));
  assertEquals(0, indexOf(darr, 0));
})();

(function() {
  const iarr = [0,1,2];

  function indexOf(arr, val) {
    return arr.indexOf(val);
  }

  %PrepareFunctionForOptimization(indexOf);
  assertEquals(0, indexOf(iarr, 0));
  assertEquals(2, indexOf(iarr, 2));

  %OptimizeFunctionOnNextCall(indexOf);

  assertEquals(0, indexOf(iarr, 0));

  const darr = [0.0, 2.0, 3.3];
  // deopt because of map change
  assertEquals(0, indexOf(darr, 0));
})();

(function() {
  const iarr = [,3];

  function indexOf(arr, val) {
    return arr.indexOf(val);
  }

  %PrepareFunctionForOptimization(indexOf);
  iarr.__proto__ = [2];
  assertEquals(-1, indexOf(iarr, 0));
  assertEquals(0, indexOf(iarr, 2));

  %OptimizeFunctionOnNextCall(indexOf);

  assertEquals(-1, indexOf(iarr, 0));

  assertEquals(0, indexOf(iarr, 2));
})();

(function() {
  const iarr = [,3];

  function indexOf(arr, val) {
    return arr.indexOf(val);
  }

  %PrepareFunctionForOptimization(indexOf);
  assertEquals(-1, indexOf(iarr, 2));
  assertEquals(1, indexOf(iarr, 3));

  %OptimizeFunctionOnNextCall(indexOf);
  assertEquals(-1, indexOf(iarr, 2));

  // deopt because of map change
  iarr.__proto__ = [2];
  assertEquals(0, indexOf(iarr, 2));
})();

// This pollutes the Array prototype, so we should not run more tests
// in the same environment after this.
(function () {
  var array = [,];

  function indexOf(val) {
    return array.indexOf(val);
  }

  %PrepareFunctionForOptimization(indexOf);
  indexOf(6); indexOf(6);

  %OptimizeFunctionOnNextCall(indexOf);
  assertEquals(indexOf(6), -1);

  array.__proto__.push(6);
  // deopt because of no_elements_protector
  assertEquals(indexOf(6), 0);
})();
