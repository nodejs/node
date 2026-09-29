// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

(function testTypedArrayAscending() {
  // Case 1: Multiple ascending constant accesses in a single block.
  function testAscending(a) {
    a[0] = 10;
    a[1] = 20;
    a[5] = 50;
    a[10] = 100;
    return a[0] + a[1] + a[5] + a[10];
  }

  const ab = new ArrayBuffer(100);
  const ta = new Int32Array(ab);

  %PrepareFunctionForOptimization(testAscending);
  assertEquals(180, testAscending(ta));
  assertEquals(180, testAscending(ta));
  %OptimizeFunctionOnNextCall(testAscending);
  assertEquals(180, testAscending(ta));
  assertTrue(isOptimized(testAscending));

  // Case 2: Out-of-bounds access on hoisted bound check causes proper deopt and
  // preserves side effects.
  const small_ab = new ArrayBuffer(24);  // length 6 for Int32Array
  const small_ta = new Int32Array(small_ab);
  testAscending(small_ta);  // Deopts because index 10 >= length 6.
  assertFalse(isOptimized(testAscending));
  // Check side-effects up to OOB were executed by Ignition after eager deopt.
  assertEquals(10, small_ta[0]);
  assertEquals(20, small_ta[1]);
  assertEquals(50, small_ta[5]);
})();

(function testTypedArrayMultipleArrays() {
  // Case 3: Multiple typed arrays in the same basic block.
  const ab = new ArrayBuffer(100);
  const ta = new Int32Array(ab);

  const ab2 = new ArrayBuffer(100);
  const ta2 = new Int32Array(ab2);
  function testMultipleArrays(a, b) {
    a[0] = 1;
    b[0] = 2;
    a[4] = 10;
    b[8] = 20;
    return a[0] + b[0] + a[4] + b[8];
  }

  %PrepareFunctionForOptimization(testMultipleArrays);
  assertEquals(33, testMultipleArrays(ta, ta2));
  assertEquals(33, testMultipleArrays(ta, ta2));
  %OptimizeFunctionOnNextCall(testMultipleArrays);
  assertEquals(33, testMultipleArrays(ta, ta2));
  assertTrue(isOptimized(testMultipleArrays));
})();

(function testTypedArrayUnordered() {
  // Case 4: Random order constant index accesses (e.g. 5, 2, 8, 1).
  function testUnordered(a) {
    a[5] = 5;
    a[2] = 2;
    a[8] = 8;
    a[1] = 1;
    return a[1] + a[2] + a[5] + a[8];
  }

  const ab = new ArrayBuffer(100);
  const ta = new Int32Array(ab);

  %PrepareFunctionForOptimization(testUnordered);
  assertEquals(16, testUnordered(ta));
  assertEquals(16, testUnordered(ta));
  %OptimizeFunctionOnNextCall(testUnordered);
  assertEquals(16, testUnordered(ta));
  assertTrue(isOptimized(testUnordered));
})();

(function testJSArray() {
  // Case 5: JSArray constant index accesses (CheckInt32Condition bounds
  // checks).
  function testJSArray(arr) {
    arr[0] = 100;
    arr[1] = 200;
    arr[3] = 300;
    return arr[0] + arr[1] + arr[3];
  }

  const js_arr = [1, 2, 3, 4, 5];
  %PrepareFunctionForOptimization(testJSArray);
  assertEquals(600, testJSArray(js_arr));
  assertEquals(600, testJSArray(js_arr));
  %OptimizeFunctionOnNextCall(testJSArray);
  assertEquals(600, testJSArray(js_arr));
  assertTrue(isOptimized(testJSArray));
})();

(function testWithConstantFolding() {
  function get_2() { return 2; }

function foo(arr) {
  // `get_2` will be non-eagerly inlined, which means that `dyn_index` will
  // initially not be a constant, but will later be folded to `3`.
  let dyn_index1 = 2 + get_2();
  let dyn_index2 = 1 + get_2();
  let dyn_index3 = 3 + get_2();

  // Inserting an initial bound check with a constant index.
  arr[0];

  // `dyn_index` will look non-constant when FindMaxConstantIndicesInBlock runs
  // the first time after `get_2` has been inlined, but the GraphOptimizer will
  // constant-fold the addition to `3`. This bound check should not be elided
  // (or at least not unless we've previously checked that `3` is in bounds).
  let result = arr[dyn_index1];
  result += arr[dyn_index2];
  result += arr[dyn_index3];
  return result;
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(get_2);
assertEquals(26, foo([1, 3, 5, 7, 8, 11, 12, 13, 14, 15]));
assertEquals(26, foo([1, 3, 5, 7, 8, 11, 12, 13, 14, 15]));

%OptimizeFunctionOnNextCall(foo);
assertEquals(26, foo([1, 3, 5, 7, 8, 11, 12, 13, 14, 15]));

// Calling with smaller array so that `arr[dyn_index]` triggers a deopt.
assertEquals(NaN, foo([1]));
})();
